const test = require('node:test');
const assert = require('node:assert');
const { groupBy } = require('../src/pkjs/utils');

test('groupBy - should group an array of objects by a string property', () => {
    const arr = [
        { type: 'fruit', name: 'apple' },
        { type: 'vegetable', name: 'carrot' },
        { type: 'fruit', name: 'banana' }
    ];
    const result = groupBy(arr, 'type');
    assert.deepStrictEqual(result, {
        fruit: [
            { type: 'fruit', name: 'apple' },
            { type: 'fruit', name: 'banana' }
        ],
        vegetable: [
            { type: 'vegetable', name: 'carrot' }
        ]
    });
});

test('groupBy - should group an array of objects by a function', () => {
    const arr = [
        { score: 10, name: 'a' },
        { score: 20, name: 'b' },
        { score: 15, name: 'c' }
    ];
    // Group by pass/fail criteria
    const result = groupBy(arr, item => item.score >= 15 ? 'pass' : 'fail');
    assert.deepStrictEqual(result, {
        fail: [
            { score: 10, name: 'a' }
        ],
        pass: [
            { score: 20, name: 'b' },
            { score: 15, name: 'c' }
        ]
    });
});

test('groupBy - should handle empty arrays', () => {
    const result = groupBy([], 'type');
    assert.deepStrictEqual(result, {});
});

test('groupBy - should handle missing properties by grouping under undefined', () => {
    const arr = [
        { type: 'fruit', name: 'apple' },
        { name: 'carrot' }, // missing 'type'
    ];
    const result = groupBy(arr, 'type');
    assert.deepStrictEqual(result, {
        fruit: [
            { type: 'fruit', name: 'apple' }
        ],
        undefined: [
            { name: 'carrot' }
        ]
    });
});

test('groupBy - should correctly handle criteria function returning undefined or null', () => {
    const arr = [
        { value: 1 },
        { value: 2 }
    ];
    const result = groupBy(arr, item => item.value === 1 ? null : undefined);
    assert.deepStrictEqual(result, {
        null: [
            { value: 1 }
        ],
        undefined: [
            { value: 2 }
        ]
    });
});
