const models = require('./models');
const storage = require('./storage');

console.log("--- BUSTING CACHE: VERSION 113 ---");

// Pebble watch-side Game IDs use signed 32-bit integers.
const MAX_SIGNED_ID = 2147483647;

// =========================================================
// GAME STATUS SORTING
// =========================================================

function getGameStatusWeight(game) {
    const time = String(game.time || "")
        .trim()
        .toLowerCase();

    // Completed games
    const isFinal =
        time === "final" ||
        time === "ft" ||
        time.indexOf("final") !== -1;

    // Scheduled games
    const isScheduled =
        time === "" ||
        time === "scheduled" ||
        time === "tba" ||
        time === "tbd" ||
        time.indexOf("am") !== -1 ||
        time.indexOf("pm") !== -1;

    // Active games first
    if (!isFinal && !isScheduled) {
        return 0;
    }

    return 1;
}

// =========================================================
// SAFE GAME ID CONVERSION
// =========================================================

function safeParseInt(val) {
    const str = String(val);

    // Preserve normal ESPN numeric IDs.
    if (/^\d+$/.test(str)) {
        const parsed = Number(str);

        if (Number.isSafeInteger(parsed) &&
            parsed > 0 &&
            parsed <= MAX_SIGNED_ID) {
            return parsed;
        }
    }

    // Deterministic fallback for non-numeric
    // or oversized identifiers.
    let hash = 0;

    for (let i = 0; i < str.length; i++) {
        hash = (
            (hash << 5) -
            hash +
            str.charCodeAt(i)
        ) | 0;
    }

    // Keep the generated ID within signed int32 range.
    const result = Math.abs(hash) % MAX_SIGNED_ID;

    return result || 1;
}

// =========================================================
// GAME LIST
// =========================================================

function sendGameList(requestID, games) {
    if (!games || games.length === 0) {
        sendEmptyGameList(requestID);
        return;
    }

    games.sort(function(a, b) {
        const weightA = getGameStatusWeight(a);
        const weightB = getGameStatusWeight(b);

        if (weightA !== weightB) {
            return weightA - weightB;
        }

        const validA =
            a.startTime instanceof Date &&
            !isNaN(a.startTime.getTime());

        const validB =
            b.startTime instanceof Date &&
            !isNaN(b.startTime.getTime());

        if (!validA || !validB) return 0;

        return a.startTime.getTime() -
               b.startTime.getTime();
    });

    const watchInfo =
        typeof Pebble !== 'undefined' &&
        typeof Pebble.getActiveWatchInfo === 'function'
            ? Pebble.getActiveWatchInfo()
            : null;

    const platform = watchInfo
        ? String(watchInfo.platform || "").toLowerCase()
        : "";

    let maxGames = 50;

    if (platform === "aplite") {
        maxGames = 5;
    } else if (
        platform === "emery" ||
        platform === "gabbro"
    ) {
        maxGames = 150;
    }

    if (games.length > maxGames) {
        console.log(
            "Capping transmitted games at " + maxGames
        );

        games = games.slice(0, maxGames);
    }

    const initDict = {
        REQUEST_ID: requestID,
        SEND_GAME_LIST: models.gameslistdata.INIT_ARRAY,
        SEND_GAME_ID: games.length
    };

    const favorites = storage.storedFavorites();

    Pebble.sendAppMessage(
        initDict,

        function() {
            sendGameListItem(
                requestID,
                games,
                0,
                favorites
            );
        },

        function(error) {
            console.log(
                "[GAME LIST] Initialization failed: " +
                JSON.stringify(error)
            );

            sendGameListError(requestID);
        }
    );
}

// =========================================================
// SEND ONE GAME AT A TIME
// =========================================================

function sendGameListItem(
    requestID,
    games,
    index,
    favorites
) {
    if (index >= games.length) return;

    const game = games[index];

    const team1Favorite = favorites.some(
        favorite =>
            game.sport == favorite.sport &&
            game.team1.id == favorite.teamID
    );

    const team2Favorite = favorites.some(
        favorite =>
            game.sport == favorite.sport &&
            game.team2.id == favorite.teamID
    );

    const isLastItem =
        index === games.length - 1;

    const dict = {
        REQUEST_ID: requestID,

        SEND_GAME_LIST: isLastItem
            ? models.gameslistdata.LAST_LIST_ITEM
            : models.gameslistdata.LIST_ITEM,

        SEND_GAME_ID: safeParseInt(game.id),
        SEND_GAME_SPORT: game.sport,

        SEND_GAME_LEAGUE:
            String(game.league || "").substring(0, 15),

        SEND_GAME_TEAM_1_NAME:
            String(game.team1.name || "").substring(0, 5),

        SEND_GAME_TEAM_2_NAME:
            String(game.team2.name || "").substring(0, 5),

        SEND_GAME_TEAM_1_ID:
            safeParseInt(game.team1.id),

        SEND_GAME_TEAM_2_ID:
            safeParseInt(game.team2.id),

        SEND_GAME_TEAM_1_SCORE:
            String(game.score1 || "").substring(0, 10),

        SEND_GAME_TEAM_2_SCORE:
            String(game.score2 || "").substring(0, 10),

        SEND_GAME_TEAM_1_RECORD:
            String(game.team1.record || "").substring(0, 10),

        SEND_GAME_TEAM_2_RECORD:
            String(game.team2.record || "").substring(0, 10),

        SEND_GAME_TEAM_1_WINNER:
            game.team1.winner ? 1 : 0,

        SEND_GAME_TEAM_2_WINNER:
            game.team2.winner ? 1 : 0,

        SEND_GAME_TEAM_1_FAVORITE:
            team1Favorite ? 1 : 0,

        SEND_GAME_TEAM_2_FAVORITE:
            team2Favorite ? 1 : 0,

        SEND_GAME_POSSESSION: game.possession,

        SEND_GAME_TIME:
            String(game.time || "").substring(0, 20),

        SEND_GAME_DETAILS:
            String(game.details || "").substring(0, 30),

        SEND_GAME_BROADCAST:
            String(game.broadcast || "").substring(0, 15)
    };

    Pebble.sendAppMessage(
        dict,

        function() {
            if (index + 1 < games.length) {
                sendGameListItem(
                    requestID,
                    games,
                    index + 1,
                    favorites
                );
            }
        },

        function(error) {
            console.log(
                "[GAME LIST] Transmission failed at " +
                index + ": " + JSON.stringify(error)
            );

            sendGameListError(requestID);
        }
    );
}

// =========================================================
// EMPTY GAME LIST
// =========================================================

function sendEmptyGameList(requestID) {
    Pebble.sendAppMessage(
        {
            REQUEST_ID: requestID,
            SEND_GAME_LIST: models.gameslistdata.NO_GAMES
        },

        function() {},

        function(error) {
            console.log(
                "[GAME LIST] Empty response failed: " +
                JSON.stringify(error)
            );
        }
    );
}

// =========================================================
// GAME LIST ERROR
// =========================================================

function sendGameListError(requestID) {
    Pebble.sendAppMessage(
        {
            REQUEST_ID: requestID,
            SEND_GAME_LIST: models.gameslistdata.NETWORK_ERROR
        },

        function() {},

        function(error) {
            console.log(
                "[GAME LIST] Error response failed: " +
                JSON.stringify(error)
            );
        }
    );
}

// =========================================================
// GAME REFRESH
// =========================================================

function sendGameUpdate(requestID, game) {
    const favorites = storage.storedFavorites();

    const team1Favorite = favorites.some(
        favorite =>
            game.sport == favorite.sport &&
            game.team1.id == favorite.teamID
    );

    const team2Favorite = favorites.some(
        favorite =>
            game.sport == favorite.sport &&
            game.team2.id == favorite.teamID
    );

    const dict = {
        REQUEST_ID: requestID,

        SEND_GAME_UPDATE:
            models.updategamedata.UPDATE_GAME,

        SEND_GAME_ID: safeParseInt(game.id),
        SEND_GAME_SPORT: game.sport,

        SEND_GAME_LEAGUE:
            String(game.league || "").substring(0, 15),

        SEND_GAME_TEAM_1_NAME:
            String(game.team1.name || "").substring(0, 5),

        SEND_GAME_TEAM_2_NAME:
            String(game.team2.name || "").substring(0, 5),

        SEND_GAME_TEAM_1_ID:
            safeParseInt(game.team1.id),

        SEND_GAME_TEAM_2_ID:
            safeParseInt(game.team2.id),

        SEND_GAME_TEAM_1_SCORE:
            String(game.score1 || "").substring(0, 10),

        SEND_GAME_TEAM_2_SCORE:
            String(game.score2 || "").substring(0, 10),

        SEND_GAME_TEAM_1_RECORD:
            String(game.team1.record || "").substring(0, 10),

        SEND_GAME_TEAM_2_RECORD:
            String(game.team2.record || "").substring(0, 10),

        SEND_GAME_TEAM_1_WINNER:
            game.team1.winner ? 1 : 0,

        SEND_GAME_TEAM_2_WINNER:
            game.team2.winner ? 1 : 0,

        SEND_GAME_TEAM_1_FAVORITE:
            team1Favorite ? 1 : 0,

        SEND_GAME_TEAM_2_FAVORITE:
            team2Favorite ? 1 : 0,

        SEND_GAME_POSSESSION: game.possession,

        SEND_GAME_TIME:
            String(game.time || "").substring(0, 20),

        SEND_GAME_DETAILS:
            String(game.details || "").substring(0, 30),

        SEND_GAME_BROADCAST:
            String(game.broadcast || "").substring(0, 15)
    };

    Pebble.sendAppMessage(
        dict,

        function() {
            console.log(
                "[GAME UPDATE] Delivered successfully"
            );
        },

        function(error) {
            console.log(
                "[GAME UPDATE] Delivery failed: " +
                JSON.stringify(error)
            );

            // Best-effort failure notification.
            sendGameUpdateError(requestID);
        }
    );
}

// =========================================================
// REFRESH ERROR
// =========================================================

function sendGameUpdateError(requestID) {
    Pebble.sendAppMessage(
        {
            REQUEST_ID: requestID,

            SEND_GAME_UPDATE:
                models.updategamedata.NETWORK_ERROR
        },

        function() {},

        function(error) {
            console.log(
                "[GAME UPDATE] Error notification failed: " +
                JSON.stringify(error)
            );
        }
    );
}

// =========================================================
// FAVORITES
// =========================================================

function sendFavoritesResult(requestID, result) {
    Pebble.sendAppMessage(
        {
            REQUEST_ID: requestID,
            CONFIRM_FAVORITE: result
        },

        function() {},

        function(error) {
            console.log(
                "[FAVORITES] Confirmation failed: " +
                JSON.stringify(error)
            );
        }
    );
}

// =========================================================
// EXPORTS
// =========================================================

module.exports = {
    sendGameList,
    sendGameListError,
    sendGameUpdate,
    sendGameUpdateError,
    sendFavoritesResult,
    getGameStatusWeight
};