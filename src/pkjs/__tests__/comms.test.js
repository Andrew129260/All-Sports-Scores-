const { getGameStatusWeight } = require('../comms');

describe('getGameStatusWeight', () => {
    it('should return 2 for "Final"', () => {
        expect(getGameStatusWeight({ time: 'Final' })).toBe(2);
    });

    it('should return 2 for "FT"', () => {
        expect(getGameStatusWeight({ time: 'FT' })).toBe(2);
    });

    it('should return 1 for time containing "am"', () => {
        expect(getGameStatusWeight({ time: '10:00 am' })).toBe(1);
    });

    it('should return 1 for time containing "pm"', () => {
        expect(getGameStatusWeight({ time: '3:00 pm' })).toBe(1);
    });

    it('should return 0 for "TBD"', () => {
        expect(getGameStatusWeight({ time: 'TBD' })).toBe(0);
    });

    it('should return 0 for empty string', () => {
        expect(getGameStatusWeight({ time: '' })).toBe(0);
    });

    it('should return 0 for undefined time', () => {
        expect(getGameStatusWeight({})).toBe(0);
    });

    it('should return 0 for null time', () => {
        expect(getGameStatusWeight({ time: null })).toBe(0);
    });
});
