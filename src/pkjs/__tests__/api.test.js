const api = require('../api');
const models = require('../models');

// Global mock for XMLHttpRequest
global.XMLHttpRequest = jest.fn();

describe('API Parsing logic', () => {
    let xhrMock;

    beforeEach(() => {
        // Reset localStorage mock
        global.localStorage = {
            getItem: jest.fn(),
            setItem: jest.fn()
        };

        xhrMock = {
            open: jest.fn(),
            send: jest.fn(),
            setRequestHeader: jest.fn(),
            readyState: 4,
            status: 200,
            responseText: ''
        };

        global.XMLHttpRequest.mockImplementation(() => xhrMock);
    });

    afterEach(() => {
        jest.clearAllMocks();
    });

    test('should parse normal flat competitions correctly', (done) => {
        const fakeData = {
            events: [
                {
                    id: "100",
                    competitions: [
                        {
                            id: "c1",
                            date: "2024-09-08T20:00:00Z",
                            competitors: [
                                { team: { abbreviation: "TEAM A", id: "1" }, score: "10" },
                                { team: { abbreviation: "TEAM B", id: "2" }, score: "5" }
                            ],
                            status: {
                                type: { name: "STATUS_FINAL" }
                            }
                        }
                    ]
                }
            ]
        };

        xhrMock.responseText = JSON.stringify(fakeData);

        // Call the API function
        // getGames(sport, leagueIndex, onLoad, onError)
        api.getGames(models.sports.NFL, 0, (games) => {
            expect(games).toHaveLength(1);
            expect(games[0].id).toBe("100");
            expect(games[0].team1.name).toBe("TEAM"); // Because 'TEAM A' gets truncated to 'TEAM ' and trimmed
            expect(games[0].team2.name).toBe("TEAM");
            expect(games[0].score1).toBe("5"); // competitor1 is index 1
            expect(games[0].score2).toBe("10"); // competitor2 is index 0
            done();
        }, () => {
            done.fail('Should not call error callback');
        });

        // Trigger xhr onload
        xhrMock.onload();
    });

    test('should filter out games scheduled more than 14 days in advance', (done) => {
        const now = new Date();
        const future15Days = new Date(now.getTime() + (15 * 24 * 60 * 60 * 1000));
        const future12Days = new Date(now.getTime() + (12 * 24 * 60 * 60 * 1000));

        const fakeData = {
            events: [
                {
                    id: "event_far",
                    competitions: [
                        {
                            id: "far",
                            date: future15Days.toISOString(),
                            competitors: [
                                { team: { abbreviation: "FAR1" } },
                                { team: { abbreviation: "FAR2" } }
                            ],
                            status: { type: { name: "STATUS_SCHEDULED" } }
                        }
                    ]
                },
                {
                    id: "event_near",
                    competitions: [
                        {
                            id: "near",
                            date: future12Days.toISOString(),
                            competitors: [
                                { team: { abbreviation: "NEAR1" } },
                                { team: { abbreviation: "NEAR2" } }
                            ],
                            status: { type: { name: "STATUS_SCHEDULED" } }
                        }
                    ]
                }
            ]
        };

        xhrMock.responseText = JSON.stringify(fakeData);

        api.getGames(models.sports.NFL, 0, (games) => {
            expect(games).toHaveLength(1);
            expect(games[0].id).toBe("event_near");
            done();
        }, () => {
            done.fail('Should not call error callback');
        });

        xhrMock.onload();
    });

    test('should parse nested groupings for Tennis correctly and skip bad matches', (done) => {
        const fakeTennisData = {
            events: [
                {
                    id: "event1",
                    name: "Grand Slam",
                    groupings: [
                        {
                            competitions: [
                                {
                                    id: "t1",
                                    date: "2024-09-08T20:00:00Z",
                                    competitors: [
                                        { athlete: { displayName: "Player 1", shortName: "P. One" }, score: "2" },
                                        { athlete: { displayName: "Player 2", shortName: "P. Two" }, score: "0" }
                                    ],
                                    status: {
                                        type: { name: "STATUS_FINAL" }
                                    }
                                },
                                {
                                    id: "t2",
                                    date: "2024-09-09T20:00:00Z",
                                    competitors: [
                                        { athlete: { displayName: "Player 3", shortName: "P. Three" } },
                                        { athlete: { displayName: "Player 4", shortName: "P. Four" } }
                                    ],
                                    status: {
                                        type: { name: "STATUS_SCHEDULED", shortDetail: "3:00 PM" }
                                    }
                                },
                                {
                                    // Should be skipped because both are TBD
                                    id: "t3",
                                    date: "2024-09-10T20:00:00Z",
                                    competitors: [
                                        { athlete: { displayName: "TBD", shortName: "TBD" } },
                                        { athlete: { displayName: "TBD", shortName: "TBD" } }
                                    ],
                                    status: { type: { name: "STATUS_SCHEDULED" } }
                                },
                                {
                                    // Should be skipped because it is retired
                                    id: "t4",
                                    date: "2024-09-08T20:00:00Z",
                                    competitors: [
                                        { athlete: { displayName: "Player 5" } },
                                        { athlete: { displayName: "Player 6" } }
                                    ],
                                    status: { type: { name: "STATUS_RETIRED" } }
                                }
                            ]
                        }
                    ]
                }
            ]
        };

        xhrMock.responseText = JSON.stringify(fakeTennisData);

        // We only fetch one endpoint for this test by specifying ATP index 0
        api.getGames(models.sports.TENNIS, 0, (games) => {
            // Assert only the valid matches (t1 and t2) are kept. t3 and t4 are dropped.
            expect(games).toHaveLength(2);

            const t1 = games.find(g => g.id === "t1");
            const t2 = games.find(g => g.id === "t2");
            const t3 = games.find(g => g.id === "t3");
            const t4 = games.find(g => g.id === "t4");

            expect(t1).toBeDefined();
            expect(t2).toBeDefined();
            expect(t3).toBeUndefined(); // Dropped
            expect(t4).toBeUndefined(); // Dropped

            expect(t1.team1.name).toBe("P. TW"); // P. Two truncated to 5 (index 1 is team1)
            expect(t1.team2.name).toBe("P. ON"); // P. One truncated to 5 (index 0 is team2)

            expect(t2.team1.name).toBe("P. FO"); // P. Four truncated to 5
            expect(t2.team2.name).toBe("P. TH"); // P. Three truncated to 5
            done();
        }, () => {
            done.fail('Should not call error callback');
        });

        // Trigger xhr onload
        xhrMock.onload();
    });
});
