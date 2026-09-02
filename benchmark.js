const { performance } = require('perf_hooks');
let mockStorage = {};
global.localStorage = {
  getItem: (key) => mockStorage[key] || null,
  setItem: (key, val) => mockStorage[key] = val
};

// Initial setup: create a large favorites map to make parsing somewhat measurable
let initialMap = {};
for (let i = 0; i < 1000; i++) {
    initialMap["sport" + i + ":team" + i] = "Team " + i + " Name";
}
global.localStorage.setItem('favoritesNames', JSON.stringify(initialMap));

// Define original functions simulating what happens in index.js
function originalRead() {
    var nameMapStr = localStorage.getItem('favoritesNames');
    var nameMap = {};
    if (nameMapStr) { try { nameMap = JSON.parse(nameMapStr); } catch(e){} }
    return nameMap;
}

function originalUpdate(key, name) {
    var nameMapStr = localStorage.getItem('favoritesNames');
    var nameMap = {};
    if (nameMapStr) { try { nameMap = JSON.parse(nameMapStr); } catch(e){} }
    nameMap[key] = name;
    localStorage.setItem('favoritesNames', JSON.stringify(nameMap));
}

// Optimized versions
var cachedFavoritesNames = null;

function getFavoritesNames() {
    if (cachedFavoritesNames !== null) {
        return cachedFavoritesNames;
    }
    var nameMapStr = localStorage.getItem('favoritesNames');
    var nameMap = {};
    if (nameMapStr) {
        try { nameMap = JSON.parse(nameMapStr); } catch (e) {}
    }
    cachedFavoritesNames = nameMap;
    return cachedFavoritesNames;
}

function saveFavoritesNames(nameMap) {
    cachedFavoritesNames = nameMap;
    localStorage.setItem('favoritesNames', JSON.stringify(nameMap));
}

function optimizedUpdate(key, name) {
    var nameMap = getFavoritesNames();
    nameMap[key] = name;
    saveFavoritesNames(nameMap);
}

// Run original benchmark
let start = performance.now();
for (let i = 0; i < 1000; i++) {
    originalRead();
}
let timeOriginalRead = performance.now() - start;

start = performance.now();
for (let i = 0; i < 1000; i++) {
    originalUpdate("newSport:team" + i, "New Team " + i);
}
let timeOriginalUpdate = performance.now() - start;

// Reset for optimized
cachedFavoritesNames = null;
global.localStorage.setItem('favoritesNames', JSON.stringify(initialMap));

start = performance.now();
for (let i = 0; i < 1000; i++) {
    getFavoritesNames();
}
let timeOptimizedRead = performance.now() - start;

start = performance.now();
for (let i = 0; i < 1000; i++) {
    optimizedUpdate("newSport:team" + i, "New Team " + i);
}
let timeOptimizedUpdate = performance.now() - start;

console.log("Original Read x1000: " + timeOriginalRead.toFixed(2) + "ms");
console.log("Original Update x1000: " + timeOriginalUpdate.toFixed(2) + "ms");
console.log("Optimized Read x1000: " + timeOptimizedRead.toFixed(2) + "ms");
console.log("Optimized Update x1000: " + timeOptimizedUpdate.toFixed(2) + "ms");
