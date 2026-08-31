const storage = require('./storage');

describe('storage.js', () => {
    let store = {};

    beforeEach(() => {
        // Mock localStorage
        store = {};
        global.localStorage = {
            getItem: jest.fn(key => store[key] || null),
            setItem: jest.fn((key, value) => {
                store[key] = value.toString();
            }),
            clear: jest.fn(() => {
                store = {};
            })
        };
    });

    afterEach(() => {
        jest.clearAllMocks();
    });

    describe('updateFavorite', () => {
        it('should add a favorite when not already present', () => {
            const favorite = { sport: 'baseball', teamID: '123' };
            const result = storage.updateFavorite(favorite);

            expect(result).toBe(true);
            const favorites = JSON.parse(localStorage.setItem.mock.calls[0][1]);
            expect(favorites).toHaveLength(1);
            expect(favorites[0]).toEqual(favorite);
        });

        it('should remove a favorite when already present', () => {
            // Need to set store directly for storedFavorites which is called inside updateFavorite,
            // and it calls getItem twice in some paths
            store['favorites'] = JSON.stringify([{ sport: 'baseball', teamID: '123' }]);

            const favorite = { sport: 'baseball', teamID: '123' };
            const result = storage.updateFavorite(favorite);

            expect(result).toBe(false);
            const favorites = JSON.parse(localStorage.setItem.mock.calls[0][1]);
            expect(favorites).toHaveLength(0);
        });

        it('should handle adding a second favorite', () => {
            store['favorites'] = JSON.stringify([{ sport: 'baseball', teamID: '123' }]);

            const favorite = { sport: 'football', teamID: '456' };
            const result = storage.updateFavorite(favorite);

            expect(result).toBe(true);
            const favorites = JSON.parse(localStorage.setItem.mock.calls[0][1]);
            expect(favorites).toHaveLength(2);
            expect(favorites).toContainEqual({ sport: 'baseball', teamID: '123' });
            expect(favorites).toContainEqual({ sport: 'football', teamID: '456' });
        });

        it('should handle removing one of multiple favorites', () => {
            store['favorites'] = JSON.stringify([
                { sport: 'baseball', teamID: '123' },
                { sport: 'football', teamID: '456' }
            ]);

            const favorite = { sport: 'football', teamID: '456' };
            const result = storage.updateFavorite(favorite);

            expect(result).toBe(false);
            const favorites = JSON.parse(localStorage.setItem.mock.calls[0][1]);
            expect(favorites).toHaveLength(1);
            expect(favorites).toContainEqual({ sport: 'baseball', teamID: '123' });
        });

        it('should handle null in localStorage by adding the favorite', () => {
            // store['favorites'] is undefined
            const favorite = { sport: 'baseball', teamID: '123' };
            const result = storage.updateFavorite(favorite);

            expect(result).toBe(true);
            const favorites = JSON.parse(localStorage.setItem.mock.calls[0][1]);
            expect(favorites).toHaveLength(1);
            expect(favorites[0]).toEqual(favorite);
        });
    });

    describe('storedFavorites', () => {
        it('should return empty array if localStorage is null', () => {
            const result = storage.storedFavorites();
            expect(result).toEqual([]);
        });

        it('should return parsed favorites if in localStorage', () => {
            const favorites = [{ sport: 'baseball', teamID: '123' }];
            store['favorites'] = JSON.stringify(favorites);

            const result = storage.storedFavorites();
            expect(result).toEqual(favorites);
        });
    });
});
