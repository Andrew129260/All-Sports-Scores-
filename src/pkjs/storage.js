let cachedFavorites = null;

function storedFavorites(forceReload = false) {
    if (forceReload) { cachedFavorites = null; }
    if (cachedFavorites !== null) {
        return cachedFavorites;
    }
    const storedOrNull = localStorage.getItem('favorites');
    if (storedOrNull == null) {
        cachedFavorites = [];
    } else {
        try {
            cachedFavorites = JSON.parse(storedOrNull);
        } catch (e) {
            cachedFavorites = [];
        }
    }
    return cachedFavorites;
}

function updateFavorite(favoriteTeam) {
    let currentFavorites = storedFavorites();
    if (currentFavorites.some(ft => ft.sport == favoriteTeam.sport && ft.teamID == favoriteTeam.teamID)) {
        //remove the updating favoriteTeam
        currentFavorites = currentFavorites.filter(ft => ft.sport != favoriteTeam.sport || ft.teamID != favoriteTeam.teamID);
        localStorage.setItem('favorites', JSON.stringify(currentFavorites));
        cachedFavorites = currentFavorites;
        return false;
    } else {
        currentFavorites.push(favoriteTeam);
        localStorage.setItem('favorites', JSON.stringify(currentFavorites));
        cachedFavorites = currentFavorites;
        return true;
    }

}

module.exports = {
    storedFavorites: storedFavorites,
    updateFavorite: updateFavorite
}
