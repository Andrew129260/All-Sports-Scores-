describe('index.js', () => {
    let pebbleListeners = {};

    beforeEach(() => {
        pebbleListeners = {};
        global.Pebble = {
            addEventListener: jest.fn((event, cb) => {
                pebbleListeners[event] = cb;
            }),
            openURL: jest.fn(),
            sendAppMessage: jest.fn(),
        };

        global.localStorage = {
            getItem: jest.fn(),
            setItem: jest.fn(),
        };

        jest.mock('../models', () => ({
            sports: { FAVORITES: 0 },
            FavoriteTeam: class { constructor(sport, teamID) { this.sport = sport; this.teamID = teamID; } }
        }));
        jest.mock('../storage', () => ({
            storedFavorites: jest.fn(() => []),
            updateFavorite: jest.fn(() => true),
            getFavoritesNames: jest.fn(() => ({})),
            saveFavoritesNames: jest.fn()
        }));
        jest.mock('../comms', () => ({
            sendGameList: jest.fn(),
            sendGameListError: jest.fn(),
            sendGameUpdate: jest.fn(),
            sendGameUpdateError: jest.fn(),
            sendFavoritesResult: jest.fn()
        }));
        jest.mock('../api', () => ({
            getGames: jest.fn(),
            getGame: jest.fn()
        }));

        jest.mock('../config.json', () => [], { virtual: true });

        // Mock message_keys to avoid resolution errors
        jest.mock('message_keys', () => ({
            FAVORITE_TEAM_SEARCH: 'FAVORITE_TEAM_SEARCH',
            CURRENT_FAVORITES: 'CURRENT_FAVORITES'
        }), { virtual: true });

        jest.resetModules();
    });

    afterEach(() => {
        jest.clearAllMocks();
    });

    it('should handle invalid JSON in localStorage for clay-settings and favoritesNames during showConfiguration', () => {
        const mockClayConfig = [{ type: 'section', items: [{ messageKey: 'CURRENT_FAVORITES', options: [] }] }];
        jest.mock('@rebble/clay', () => {
            return jest.fn().mockImplementation(() => ({
                config: mockClayConfig,
                generateUrl: jest.fn(() => 'http://example.com'),
                getSettings: jest.fn(() => ({}))
            }));
        }, { virtual: true });

        require('../index.js');

        // Mock localStorage to return invalid JSON
        global.localStorage.getItem.mockImplementation((key) => {
            if (key === 'clay-settings') return '{ invalid_json';
            if (key === 'favoritesNames') return '{ invalid_json';
            return null;
        });

        // Trigger showConfiguration
        expect(() => pebbleListeners['showConfiguration']({})).not.toThrow();

        // It should not throw an error and should set clay-settings in localStorage to a valid JSON string
        expect(global.localStorage.setItem).toHaveBeenCalledWith('clay-settings', JSON.stringify({
            CURRENT_FAVORITES: [],
            FAVORITE_TEAM_SEARCH: ""
        }));
    });

    it('should handle valid JSON in localStorage for clay-settings and favoritesNames during showConfiguration', () => {
        const mockClayConfig = [{ type: 'section', items: [{ messageKey: 'CURRENT_FAVORITES', options: [] }] }];
        jest.mock('@rebble/clay', () => {
            return jest.fn().mockImplementation(() => ({
                config: mockClayConfig,
                generateUrl: jest.fn(() => 'http://example.com'),
                getSettings: jest.fn(() => ({}))
            }));
        }, { virtual: true });

        require('../index.js');

        // Mock localStorage to return valid JSON
        global.localStorage.getItem.mockImplementation((key) => {
            if (key === 'clay-settings') return JSON.stringify({ 'PREVIOUS_SETTING': 'value' });
            if (key === 'favoritesNames') return JSON.stringify({ '0:123': 'Test Team' });
            return null;
        });

        // Trigger showConfiguration
        expect(() => pebbleListeners['showConfiguration']({})).not.toThrow();

        // It should merge settings correctly
        expect(global.localStorage.setItem).toHaveBeenCalledWith('clay-settings', JSON.stringify({
            PREVIOUS_SETTING: 'value',
            CURRENT_FAVORITES: [],
            FAVORITE_TEAM_SEARCH: ""
        }));
    });

    it('should handle invalid JSON in localStorage for favoritesNames during webviewclosed', () => {
        jest.mock('@rebble/clay', () => {
            return jest.fn().mockImplementation(() => ({
                config: [],
                generateUrl: jest.fn(() => 'http://example.com'),
                getSettings: jest.fn(() => ({}))
            }));
        }, { virtual: true });

        require('../index.js');

        // Mock localStorage to return invalid JSON
        global.localStorage.getItem.mockImplementation((key) => {
            if (key === 'favoritesNames') return '{ invalid_json2';
            return null;
        });

        const mockResponse = JSON.stringify({
            CURRENT_FAVORITES: ['0:123'],
            FAVORITE_TEAM_SEARCH: 'Eagles'
        });

        global.XMLHttpRequest = jest.fn(() => ({
            open: jest.fn(),
            send: jest.fn(),
            onload: jest.fn(),
            onerror: jest.fn()
        }));

        // Trigger webviewclosed
        expect(() => pebbleListeners['webviewclosed']({ response: encodeURIComponent(mockResponse) })).not.toThrow();
    });

    it('should handle valid JSON in localStorage for favoritesNames during webviewclosed', () => {
        jest.mock('@rebble/clay', () => {
            return jest.fn().mockImplementation(() => ({
                config: [],
                generateUrl: jest.fn(() => 'http://example.com'),
                getSettings: jest.fn(() => ({}))
            }));
        }, { virtual: true });

        require('../index.js');

        // Mock localStorage to return valid JSON
        global.localStorage.getItem.mockImplementation((key) => {
            if (key === 'favoritesNames') return JSON.stringify({ "0:123": "Existing Team" });
            return null;
        });

        const mockResponse = JSON.stringify({
            CURRENT_FAVORITES: ['0:123'],
            FAVORITE_TEAM_SEARCH: ''
        });

        global.XMLHttpRequest = jest.fn(() => ({
            open: jest.fn(),
            send: jest.fn(),
            onload: jest.fn(),
            onerror: jest.fn()
        }));

        // Trigger webviewclosed
        expect(() => pebbleListeners['webviewclosed']({ response: encodeURIComponent(mockResponse) })).not.toThrow();
    });

    it('should handle invalid JSON in localStorage for favoritesNames during appmessage ADD_FAVORITE_SPORT', () => {
        jest.mock('@rebble/clay', () => {
            return jest.fn().mockImplementation(() => ({}));
        }, { virtual: true });

        require('../index.js');

        // Mock localStorage to return invalid JSON
        global.localStorage.getItem.mockImplementation((key) => {
            if (key === 'favoritesNames') return '{ invalid_json3';
            return null;
        });

        const mockPayload = {
            REQUEST_ID: 1,
            ADD_FAVORITE_SPORT: 0,
            ADD_FAVORITE_TEAM_ID: 123
        };

        // Trigger appmessage
        expect(() => pebbleListeners['appmessage']({ payload: mockPayload })).not.toThrow();

        expect(require('../storage').saveFavoritesNames).toHaveBeenCalledWith({
            "0:123": "Team 123 (Sport 0)"
        });
        // expect(global.localStorage.setItem).toHaveBeenCalledWith('favoritesNames', JSON.stringify({
        //    "0:123": "Team 123 (Sport 0)"
        // }));
    });

    it('should handle appmessage remove favorite (added=false) with invalid JSON', () => {
        jest.mock('@rebble/clay', () => {
            return jest.fn().mockImplementation(() => ({}));
        }, { virtual: true });

        // Override updateFavorite to return false (removed)
        jest.mock('../storage', () => ({
            storedFavorites: jest.fn(() => []),
            updateFavorite: jest.fn(() => false),
            getFavoritesNames: jest.fn(() => ({})),
            saveFavoritesNames: jest.fn()
        }));

        require('../index.js');

        // Mock localStorage to return invalid JSON
        global.localStorage.getItem.mockImplementation((key) => {
            if (key === 'favoritesNames') return '{ invalid_json4';
            return null;
        });

        const mockPayload = {
            REQUEST_ID: 1,
            ADD_FAVORITE_SPORT: 0,
            ADD_FAVORITE_TEAM_ID: 123
        };

        // Trigger appmessage
        expect(() => pebbleListeners['appmessage']({ payload: mockPayload })).not.toThrow();

        expect(require('../storage').saveFavoritesNames).toHaveBeenCalledWith({});
        // expect(global.localStorage.setItem).toHaveBeenCalledWith('favoritesNames', JSON.stringify({}));
    });
});
