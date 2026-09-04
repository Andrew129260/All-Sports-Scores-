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

        // Mock message_keys to avoid resolution errors
        jest.mock('message_keys', () => ({
            CURRENT_FAVORITES: 'CURRENT_FAVORITES'
        }), { virtual: true });

        jest.resetModules();
    });

    afterEach(() => {
        jest.clearAllMocks();
    });

    it('should handle invalid JSON in localStorage for favoritesNames during appmessage ADD_FAVORITE_SPORT', () => {
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
    });

    it('should handle appmessage remove favorite (added=false) with invalid JSON', () => {
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
    });
});
