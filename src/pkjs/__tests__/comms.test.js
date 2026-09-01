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
