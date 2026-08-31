const api = require('../src/pkjs/api.js');
const models = require('../src/pkjs/models.js');

describe('API Tests', () => {
    beforeEach(() => {
        // Clear mocks before each test
        jest.clearAllMocks();

        // Setup console.log spy
        jest.spyOn(console, 'log').mockImplementation(() => {});

        // Mock XMLHttpRequest
        global.XMLHttpRequest = jest.fn(() => ({
            open: jest.fn(),
            send: jest.fn(),
            setRequestHeader: jest.fn(),
            readyState: 4,
            status: 200,
            responseText: 'invalid json',
        }));
    });

    afterEach(() => {
        jest.restoreAllMocks();
        delete global.XMLHttpRequest;
    });

    it('should catch JSON parse errors in executeFetchTasks', () => {
        // Mock XMLHttpRequest constructor to simulate the callback execution immediately after send
        global.XMLHttpRequest = jest.fn().mockImplementation(function() {
            this.open = jest.fn();
            this.send = jest.fn(() => {
                // Simulate an onload callback call synchronously
                this.readyState = 4;
                this.status = 200;
                this.responseText = '{ invalid json }'; // Unparseable JSON
                if (this.onload) {
                    this.onload();
                }
            });
            this.setRequestHeader = jest.fn();
        });

        // Use a real sport constant
        const sport = models.sports.NFL;

        // Mock the callbacks
        const onLoadMock = jest.fn();
        const onErrorMock = jest.fn();

        // Call the method that triggers the fetch
        api.getGames(sport, 0, onLoadMock, onErrorMock);

        // Verify that console.log was called with the error message
        expect(console.log).toHaveBeenCalledWith(expect.stringMatching(/JSON Parse Error for: /));
        expect(onLoadMock).toHaveBeenCalledWith([]);
    });
});
