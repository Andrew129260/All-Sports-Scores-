const models = require('./models');
const storage = require('./storage');

console.log("--- BUSTING CACHE: VERSION 112 ---");

function getGameStatusWeight(game) {
    if (game.time === "Final" || game.time === "FT") return 2; 
    if (game.time && (game.time.indexOf("am") > -1 || game.time.indexOf("pm") > -1)) return 1; 
    return 0; 
}

function safeParseInt(val) {
    let parsed = parseInt(val);
    if (!isNaN(parsed) && parsed > 0) return parsed;
    
    let hash = 0;
    let str = String(val);
    for (let i = 0; i < str.length; i++) {
        hash = ((hash << 5) - hash) + str.charCodeAt(i);
        hash |= 0; 
    }
    return Math.abs(hash) || 0;
}

function sendGameList(requestID, games) {
    if (games.length == 0) {
        sendEmptyGameList(requestID);
    } else {
        games.sort(function(a, b) {
            var weightA = getGameStatusWeight(a);
            var weightB = getGameStatusWeight(b);
            
            if (weightA !== weightB) {
                return weightA - weightB; 
            }
            
            if (!a.startTime || isNaN(a.startTime.getTime()) || !b.startTime || isNaN(b.startTime.getTime())) return 0;
            
            if (weightA === 2) {
                return b.startTime.getTime() - a.startTime.getTime();
            } else {
                return a.startTime.getTime() - b.startTime.getTime();
            }
        });

        var watchInfo = typeof Pebble !== 'undefined' && Pebble.getActiveWatchInfo ? Pebble.getActiveWatchInfo() : null;
        var isAplite = watchInfo && watchInfo.platform === 'aplite';

        if (isAplite) {
            const MAX_GAMES = 5; 
            if (games.length > MAX_GAMES) {
                console.log("Aplite Detected: Capping payload at " + MAX_GAMES + " games.");
                games = games.slice(0, MAX_GAMES);
            }
        }

        var initDict = {
            'REQUEST_ID': requestID,
            'SEND_GAME_LIST': models.gameslistdata.INIT_ARRAY, 
            'SEND_GAME_ID': games.length 
        };

        Pebble.sendAppMessage(initDict, function() {
            sendGameListItem(requestID, games, 0);
        }, function() {
            console.log('Failed to initialize array on watch');
            sendGameListError(requestID); 
        });
    }
}

function sendGameListItem(requestID, games, index) {
    const game = games[index];

    const favorites = storage.storedFavorites();
    const team1Favorite = favorites.some(favoriteTeam => game.sport == favoriteTeam.sport && game.team1.id == favoriteTeam.teamID);
    const team2Favorite = favorites.some(favoriteTeam => game.sport == favoriteTeam.sport && game.team2.id == favoriteTeam.teamID);
    
    const isLastItem = index == games.length - 1;

    const dict = {
        'REQUEST_ID': requestID,
        'SEND_GAME_LIST': isLastItem ? models.gameslistdata.LAST_LIST_ITEM : models.gameslistdata.LIST_ITEM,
        'SEND_GAME_ID': safeParseInt(game.id),
        'SEND_GAME_SPORT': game.sport,
        'SEND_GAME_LEAGUE': String(game.league || "").substring(0, 15), 
        'SEND_GAME_TEAM_1_NAME': String(game.team1.name || "").substring(0, 5),
        'SEND_GAME_TEAM_2_NAME': String(game.team2.name || "").substring(0, 5),
        'SEND_GAME_TEAM_1_ID': safeParseInt(game.team1.id),
        'SEND_GAME_TEAM_2_ID': safeParseInt(game.team2.id),
        'SEND_GAME_TEAM_1_SCORE': String(game.score1 || "").substring(0, 10),
        'SEND_GAME_TEAM_2_SCORE': String(game.score2 || "").substring(0, 10),
        'SEND_GAME_TEAM_1_RECORD': String(game.team1.record || "").substring(0, 10),
        'SEND_GAME_TEAM_2_RECORD': String(game.team2.record || "").substring(0, 10),
        'SEND_GAME_TEAM_1_FAVORITE': team1Favorite ? 1 : 0,
        'SEND_GAME_TEAM_2_FAVORITE': team2Favorite ? 1 : 0,
        'SEND_GAME_POSSESSION': game.possession,
        'SEND_GAME_TIME': String(game.time || "").substring(0, 20),
        'SEND_GAME_DETAILS': String(game.details || "").substring(0, 30),
        'SEND_GAME_BROADCAST': String(game.broadcast || "").substring(0, 15) 
    };

    Pebble.sendAppMessage(dict, function () {
        index++;
        if (index < games.length) {
            sendGameListItem(requestID, games, index);
        }
    }, function () {
        console.log('Item transmission failed at index: ' + index);
        sendGameListError(requestID); 
    });
}

function sendEmptyGameList(requestID) {
    var dict = {
        'REQUEST_ID': requestID,
        'SEND_GAME_LIST': models.gameslistdata.NO_GAMES
    };
    Pebble.sendAppMessage(dict, function () {}, function () {});
}

function sendGameListError(requestID) {
    var dict = {
        'REQUEST_ID': requestID,
        'SEND_GAME_LIST': models.gameslistdata.NETWORK_ERROR
    };
    Pebble.sendAppMessage(dict, function () {}, function () {});
}

function sendGameUpdate(requestID, game) {
    const favorites = storage.storedFavorites();
    const team1Favorite = favorites.some(favoriteTeam => game.sport == favoriteTeam.sport && game.team1.id == favoriteTeam.teamID);
    const team2Favorite = favorites.some(favoriteTeam => game.sport == favoriteTeam.sport && game.team2.id == favoriteTeam.teamID);

    const dict = {
        'REQUEST_ID': requestID,
        'SEND_GAME_UPDATE': models.updategamedata.UPDATE_GAME,
        'SEND_GAME_ID': safeParseInt(game.id),
        'SEND_GAME_SPORT': game.sport,
        'SEND_GAME_LEAGUE': String(game.league || "").substring(0, 15), 
        'SEND_GAME_TEAM_1_NAME': String(game.team1.name || "").substring(0, 5),
        'SEND_GAME_TEAM_2_NAME': String(game.team2.name || "").substring(0, 5),
        'SEND_GAME_TEAM_1_ID': safeParseInt(game.team1.id),
        'SEND_GAME_TEAM_2_ID': safeParseInt(game.team2.id),
        'SEND_GAME_TEAM_1_SCORE': String(game.score1 || "").substring(0, 10),
        'SEND_GAME_TEAM_2_SCORE': String(game.score2 || "").substring(0, 10),
        'SEND_GAME_TEAM_1_RECORD': String(game.team1.record || "").substring(0, 10),
        'SEND_GAME_TEAM_2_RECORD': String(game.team2.record || "").substring(0, 10),
        'SEND_GAME_TEAM_1_FAVORITE': team1Favorite ? 1 : 0,
        'SEND_GAME_TEAM_2_FAVORITE': team2Favorite ? 1 : 0,
        'SEND_GAME_POSSESSION': game.possession,
        'SEND_GAME_TIME': String(game.time || "").substring(0, 20),
        'SEND_GAME_DETAILS': String(game.details || "").substring(0, 30),
        'SEND_GAME_BROADCAST': String(game.broadcast || "").substring(0, 15) 
    };
    Pebble.sendAppMessage(dict, function () {}, function () {});
}

function sendGameUpdateError(requestID) {
    var dict = {
        'REQUEST_ID': requestID,
        'SEND_GAME_UPDATE': models.updategamedata.NETWORK_ERROR
    };
    Pebble.sendAppMessage(dict, function () {}, function () {});
}

function sendFavoritesResult(requestID, result) {
    var dict = {
        'REQUEST_ID': requestID,
        'CONFIRM_FAVORITE': result
    };
    Pebble.sendAppMessage(dict, function () {}, function () {});
}

module.exports = {
    sendGameList,
    sendGameListError,
    sendGameUpdate,
    sendGameUpdateError,
    sendFavoritesResult
};
