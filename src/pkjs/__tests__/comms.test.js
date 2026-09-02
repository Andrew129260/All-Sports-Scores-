const { getGameStatusWeight } = require('../comms');

describe('getGameStatusWeight', () => {
    it('should return 1 for "Final"', () => {
        expect(getGameStatusWeight({ time: 'Final' })).toBe(1);
    });

    it('should return 1 for "FT"', () => {
        expect(getGameStatusWeight({ time: 'FT' })).toBe(1);
    });

    it('should return 1 for time containing "am"', () => {
        expect(getGameStatusWeight({ time: '10:00 am' })).toBe(1);
    });

    it('should return 1 for time containing "pm"', () => {
        expect(getGameStatusWeight({ time: '3:00 pm' })).toBe(1);
    });

    it('should return 1 for "TBD"', () => {
        expect(getGameStatusWeight({ time: 'TBD' })).toBe(1);
    });

    it('should return 1 for empty string', () => {
        expect(getGameStatusWeight({ time: '' })).toBe(1);
    });

    it('should return 1 for undefined time', () => {
        expect(getGameStatusWeight({})).toBe(1);
    });

    it('should return 1 for null time', () => {
        expect(getGameStatusWeight({ time: null })).toBe(1);
    });

    it('should return 0 for active games (e.g. 1st Qtr)', () => {
        expect(getGameStatusWeight({ time: '1st Qtr' })).toBe(0);
    });

    it('should return 0 for active games (e.g. 2nd Half)', () => {
        expect(getGameStatusWeight({ time: '2nd Half' })).toBe(0);
    });
});

describe('sendGameList', () => {
    let comms;
    let storage;
    let mockSendAppMessage;

    beforeEach(() => {
        jest.resetModules();
        comms = require('../comms');
        storage = require('../storage');

        mockSendAppMessage = jest.fn((dict, success, error) => {
            if (success) success();
        });

        global.Pebble = {
            sendAppMessage: mockSendAppMessage,
            getActiveWatchInfo: jest.fn(() => ({ platform: 'diorite' }))
        };

        jest.spyOn(storage, 'storedFavorites').mockReturnValue([]);
    });

    afterEach(() => {
        jest.clearAllMocks();
        delete global.Pebble;
    });

    it('should send NO_GAMES if games list is empty', () => {
        comms.sendGameList(123, []);

        expect(mockSendAppMessage).toHaveBeenCalledTimes(1);
        expect(mockSendAppMessage.mock.calls[0][0]).toMatchObject({
            'REQUEST_ID': 123,
            'SEND_GAME_LIST': 2 // models.gameslistdata.NO_GAMES
        });
    });

    it('should initialize array and send items sequentially', () => {
        const game1 = {
            id: 1, sport: 1, team1: { id: 1 }, team2: { id: 2 },
            startTime: new Date('2023-01-01T12:00:00Z'), time: 'Final'
        };
        const game2 = {
            id: 2, sport: 1, team1: { id: 3 }, team2: { id: 4 },
            startTime: new Date('2023-01-01T13:00:00Z'), time: '1st Qtr'
        };

        comms.sendGameList(123, [game1, game2]);

        // Due to recursive callbacks mimicking Pebble's async nature, they run synchronously here
        // Initial call + 2 items
        expect(mockSendAppMessage).toHaveBeenCalledTimes(3);

        // 1. INIT_ARRAY
        expect(mockSendAppMessage.mock.calls[0][0]).toMatchObject({
            'REQUEST_ID': 123,
            'SEND_GAME_LIST': 4, // INIT_ARRAY
            'SEND_GAME_ID': 2
        });

        // 2. LIST_ITEM (game2 - active, so it sorts first)
        expect(mockSendAppMessage.mock.calls[1][0]).toMatchObject({
            'REQUEST_ID': 123,
            'SEND_GAME_LIST': 0, // LIST_ITEM
            'SEND_GAME_ID': 2
        });

        // 3. LAST_LIST_ITEM (game1 - final, so it sorts last)
        expect(mockSendAppMessage.mock.calls[2][0]).toMatchObject({
            'REQUEST_ID': 123,
            'SEND_GAME_LIST': 1, // LAST_LIST_ITEM
            'SEND_GAME_ID': 1
        });
    });

    it('should cap payload at 5 games if platform is aplite', () => {
        global.Pebble.getActiveWatchInfo.mockReturnValue({ platform: 'aplite' });

        const games = Array.from({ length: 10 }, (_, i) => ({
            id: i, sport: 1, team1: { id: 1 }, team2: { id: 2 },
            startTime: new Date(`2023-01-01T1${i}:00:00Z`), time: 'Final'
        }));

        comms.sendGameList(123, games);

        // 1 init + 5 items
        expect(mockSendAppMessage).toHaveBeenCalledTimes(6);
        expect(mockSendAppMessage.mock.calls[0][0]).toMatchObject({
            'SEND_GAME_ID': 5 // Number of games sent
        });
    });

    it('should sort chronologically for games with same weight', () => {
        const game1 = {
            id: 1, sport: 1, team1: { id: 1 }, team2: { id: 2 },
            startTime: new Date('2023-01-02T12:00:00Z'), time: 'Final'
        };
        const game2 = {
            id: 2, sport: 1, team1: { id: 3 }, team2: { id: 4 },
            startTime: new Date('2023-01-01T12:00:00Z'), time: 'Final'
        };

        comms.sendGameList(123, [game1, game2]);

        // game2 should be first chronologically
        expect(mockSendAppMessage.mock.calls[1][0]).toMatchObject({
            'SEND_GAME_ID': 2
        });
        expect(mockSendAppMessage.mock.calls[2][0]).toMatchObject({
            'SEND_GAME_ID': 1
        });
    });

    it('should send network error if sending init fails', () => {
        mockSendAppMessage = jest.fn((dict, success, error) => {
            if (error) error();
        });
        global.Pebble.sendAppMessage = mockSendAppMessage;

        const game = {
            id: 1, sport: 1, team1: { id: 1 }, team2: { id: 2 },
            startTime: new Date(), time: 'Final'
        };

        comms.sendGameList(123, [game]);

        // 1 init (fails) + 1 error
        expect(mockSendAppMessage).toHaveBeenCalledTimes(2);
        expect(mockSendAppMessage.mock.calls[1][0]).toMatchObject({
            'REQUEST_ID': 123,
            'SEND_GAME_LIST': 3 // NETWORK_ERROR
        });
    });

    it('should send network error if sending item fails', () => {
        let callCount = 0;
        mockSendAppMessage = jest.fn((dict, success, error) => {
            let currentCall = callCount++;
            if (currentCall === 0) {
                if (success) success(); // init succeeds
            } else {
                if (error) error(); // item fails
            }
        });
        global.Pebble.sendAppMessage = mockSendAppMessage;

        const game = {
            id: 1, sport: 1, team1: { id: 1 }, team2: { id: 2 },
            startTime: new Date(), time: 'Final'
        };

        comms.sendGameList(123, [game]);

        // 1 init + 1 item (fails) + 1 error
        expect(mockSendAppMessage).toHaveBeenCalledTimes(3);
        expect(mockSendAppMessage.mock.calls[2][0]).toMatchObject({
            'REQUEST_ID': 123,
            'SEND_GAME_LIST': 3 // NETWORK_ERROR
        });
    });

    it('should handle favorites correctly during sending', () => {
        storage.storedFavorites.mockReturnValue([
            { sport: 1, teamID: 1 } // team 1 is favorite
        ]);

        const game = {
            id: 1, sport: 1,
            team1: { id: 1 },
            team2: { id: 2 },
            startTime: new Date(), time: 'Final'
        };

        comms.sendGameList(123, [game]);

        expect(mockSendAppMessage.mock.calls[1][0]).toMatchObject({
            'SEND_GAME_TEAM_1_FAVORITE': 1,
            'SEND_GAME_TEAM_2_FAVORITE': 0
        });
    });
});
