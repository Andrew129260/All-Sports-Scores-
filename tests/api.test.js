const { parseEvent } = require('../src/pkjs/api');
const models = require('../src/pkjs/models');

describe('api.js - parseEvent', () => {
    it('returns null if there are no competitions', () => {
        const sport = 1;
        const league = 'MLB';
        const event = {
            competitions: []
        };
        const result = parseEvent(sport, league, event);
        expect(result).toBeNull();
    });

    it('returns null if competitions is null or undefined', () => {
        const result = parseEvent(1, 'MLB', {});
        expect(result).toBeNull();
    });

    it('parses a scheduled event correctly', () => {
        const sport = models.sports.BASEBALL;
        const league = 'MLB';
        const event = {
            id: "12345",
            competitions: [{
                date: "2023-10-15T20:00:00Z",
                status: { type: { name: "STATUS_SCHEDULED" } },
                competitors: [
                    { team: { abbreviation: "NYY", id: "1" }, records: [{ summary: "90-72" }] },
                    { team: { abbreviation: "BOS", id: "2" }, records: [{ summary: "78-84" }] }
                ],
                broadcasts: [{ names: ["ESPN"] }]
            }]
        };

        const result = parseEvent(sport, league, event);
        expect(result).not.toBeNull();
        expect(result.id).toBe("12345");
        expect(result.sport).toBe(sport);

        // Competitor 1 is index 1, competitor 2 is index 0
        expect(result.team1.name).toBe("BOS");
        expect(result.team1.id).toBe("2");
        expect(result.team1.record).toBe("78-84");
        expect(result.score1).toBe(""); // Scheduled so no score

        expect(result.team2.name).toBe("NYY");
        expect(result.team2.id).toBe("1");
        expect(result.team2.record).toBe("90-72");
        expect(result.score2).toBe("");

        expect(result.broadcast).toBe("ESPN");
    });

    it('parses a final event correctly and assigns scores', () => {
        const sport = models.sports.BASKETBALL;
        const league = 'NBA';
        const event = {
            id: "54321",
            competitions: [{
                date: "2023-10-15T20:00:00Z",
                status: { type: { name: "STATUS_FINAL", shortDetail: "Final" } },
                competitors: [
                    { team: { abbreviation: "LAL", id: "3" }, score: "110", records: [{ summary: "1-0" }] },
                    { team: { abbreviation: "GSW", id: "4" }, score: "105", records: [{ summary: "0-1" }] }
                ]
            }]
        };

        const result = parseEvent(sport, league, event);
        expect(result).not.toBeNull();
        expect(result.time).toBe("Final");

        expect(result.team1.name).toBe("GSW");
        expect(result.score1).toBe("105");

        expect(result.team2.name).toBe("LAL");
        expect(result.score2).toBe("110");
    });

    it('handles special sport/league local time parsing (e.g., sport 7)', () => {
        const sport = 7; // Soccer
        const league = 'EPL';
        const date = new Date("2023-10-15T20:30:00Z");

        const event = {
            id: "999",
            competitions: [{
                date: date.toISOString(),
                status: { type: { name: "STATUS_SCHEDULED" } },
                competitors: [
                    { team: { abbreviation: "MUN", id: "5" } },
                    { team: { abbreviation: "ARS", id: "6" } }
                ]
            }]
        };

        const result = parseEvent(sport, league, event);
        expect(result).not.toBeNull();

        let expectedHours = date.getHours();
        let expectedMinutes = date.getMinutes();
        let expectedAmPm = expectedHours >= 12 ? 'PM' : 'AM';
        expectedHours = expectedHours % 12;
        expectedHours = expectedHours ? expectedHours : 12;
        expectedMinutes = expectedMinutes < 10 ? '0' + expectedMinutes : expectedMinutes;
        const expectedTime = expectedHours + ':' + expectedMinutes + ' ' + expectedAmPm;

        expect(result.time).toBe(expectedTime);
    });

    it('formats cricket scores correctly', () => {
        const sport = models.sports.CRICKET;
        const league = 'IPL';
        const event = {
            id: "777",
            competitions: [{
                date: "2023-10-15T20:00:00Z",
                status: { type: { name: "STATUS_FINAL" } },
                competitors: [
                    { team: { abbreviation: "MI", id: "7" }, score: "150/5 & 120/4" },
                    { team: { abbreviation: "CSK", id: "8" }, score: "140/6 (20 ov)" }
                ]
            }]
        };

        const result = parseEvent(sport, league, event);
        expect(result).not.toBeNull();

        // CSK is competitor 1
        expect(result.team1.name).toBe("CSK");
        expect(result.score1).toBe("140/6"); // Should strip " (20 ov)"

        // MI is competitor 2
        expect(result.team2.name).toBe("MI");
        expect(result.score2).toBe("120/4"); // Should keep latest innings after "&"
    });

    it('truncates long team abbreviations to 5 characters', () => {
        const event = {
            competitions: [{
                date: "2023-10-15T20:00:00Z",
                competitors: [
                    { team: { abbreviation: "LONGER", id: "9" } },
                    { team: { abbreviation: "SHORTR", id: "10" } }
                ]
            }]
        };

        const result = parseEvent(1, 'TEST', event);
        expect(result).not.toBeNull();

        // Competitor 1 (SHORTR) -> team1
        expect(result.team1.name).toBe("SHORT");

        // Competitor 2 (LONGER) -> team2
        expect(result.team2.name).toBe("LONGE");
    });
});
