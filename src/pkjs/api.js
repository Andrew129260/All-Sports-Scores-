/*
 * All Sports Scores - ESPN data layer
 * -----------------------------------
 *
 * This file intentionally keeps all ESPN/network behavior on the phone side.
 * The watch only receives compact, already-parsed game dictionaries.
 *
 * Important design rules used here:
 *
 * 1. Keep watch payloads bounded.
 *    Older Pebbles have very little AppMessage/heap space, so the phone caps
 *    game counts before sending anything to the watch.
 *
 * 2. Treat ESPN endpoints as best-effort, not as a stable public API.
 *    Static league endpoints are retained as fallbacks, while dynamic header
 *    discovery is used where ESPN moves active competitions around.
 *
 * 3. Never let a stale start timestamp hide a game that is actually LIVE.
 *    This matters especially for multi-day Cricket Test matches.
 *
 * 4. Past folders avoid fragile multi-day ranges.
 *    Tennis/IPL/MLC use single-day requests. International Cricket Past uses
 *    recently discovered numeric series IDs instead of the obsolete 8039/8040
 *    buckets.
 *
 * 5. Past folders are strictly historical.
 *    ESPN Tennis scoreboards sometimes return live/current matches even when
 *    an old dates=YYYYMMDD value was requested. Explicit Past folders reject
 *    LIVE games and require a usable historical start time.
 *
 * 6. Favorites/global loads never fetch explicit Past folders.
 *    This prevents background favorite/timeline refreshes from multiplying
 *    historical requests and keeps the normal app startup path lightweight.
 *
 * 7. Game Info uses sport-specific terminology.
 *    Innings, sets, quarters, periods and rounds are labeled appropriately,
 *    and Cricket innings prefer runs/wickets rather than ESPN period numbers.
 */

const models = require('./models');
const storage = require('./storage');
const utils = require('./utils');
const mock = require('./mock');

const DEBUG_MOCK = false;

const MAX_GAMES_DEFAULT = 50;
const MAX_GAMES_APLITE = 5;
const MAX_GAMES_HIGH_MEMORY = 150;


/*
 * Scoreboard URL cache used by sports whose Game Info data is easiest to
 * retrieve from the exact scoreboard that produced the game (notably
 * Cricket and Tennis).
 *
 * The cache stores URLs without the transient `t=` cache-buster so Game Info
 * can add a fresh one later.
 */
let gameScoreboardUrls = {};


/*
 * CRICKET SERIES DISCOVERY CACHE
 * ------------------------------
 *
 * ESPN's personalized Cricket header is the best source for active series
 * IDs, but our real-world logs show that endpoint can intermittently return
 * 502/504. The Site API scoreboards for the numeric series IDs are much more
 * reliable once those IDs are known.
 *
 * Keep a rolling cache of recently discovered series on the phone so:
 *
 *   - International can survive a temporary header outage.
 *   - Int'l Past can still query series that disappeared from today's header
 *     a few days ago.
 *
 * Each entry is timestamped individually and pruned after 21 days. That is
 * intentionally a little longer than the 14-day Past window.
 */
const CRICKET_SERIES_CACHE_KEY =
    "cricket_series_cache_v1";

const CRICKET_SERIES_CACHE_MAX_AGE_MS =
    21 * 24 * 60 * 60 * 1000;

const CRICKET_SERIES_CACHE_MAX_ENTRIES =
    64;


function loadCricketSeriesCache() {
    try {
        const raw =
            localStorage.getItem(
                CRICKET_SERIES_CACHE_KEY
            );

        if (!raw) {
            return [];
        }

        const parsed =
            JSON.parse(raw);

        if (!Array.isArray(parsed)) {
            return [];
        }

        const cutoff =
            Date.now() -
            CRICKET_SERIES_CACHE_MAX_AGE_MS;

        return parsed
            .filter(entry => {
                return (
                    entry &&
                    entry.id &&
                    entry.seenAt &&
                    entry.seenAt >= cutoff
                );
            })
            .slice(
                0,
                CRICKET_SERIES_CACHE_MAX_ENTRIES
            );

    } catch (e) {
        console.log(
            "[CRICKET CACHE] Load failed: " +
            e
        );

        return [];
    }
}


function saveCricketSeriesCache(activeLeagues) {
    if (
        !activeLeagues ||
        activeLeagues.length === 0
    ) {
        return;
    }

    const now =
        Date.now();

    const byId = {};


    /*
     * Start with still-valid cached entries.
     */
    loadCricketSeriesCache()
        .forEach(entry => {
            byId[String(entry.id)] =
                entry;
        });


    /*
     * Refresh every series ESPN currently reports.
     */
    activeLeagues.forEach(league => {
        if (!league || !league.id) {
            return;
        }

        const id =
            String(league.id);

        byId[id] = {
            id:
                id,

            league:
                league.abbreviation ||
                league.shortName ||
                league.name ||
                "International",

            seenAt:
                now
        };
    });


    const merged =
        Object.keys(byId)
            .map(id => byId[id])
            .sort((a, b) => {
                return (
                    b.seenAt -
                    a.seenAt
                );
            })
            .slice(
                0,
                CRICKET_SERIES_CACHE_MAX_ENTRIES
            );


    try {
        localStorage.setItem(
            CRICKET_SERIES_CACHE_KEY,
            JSON.stringify(merged)
        );

        console.log(
            "[CRICKET CACHE] Saved " +
            merged.length +
            " recent series."
        );

    } catch (e) {
        console.log(
            "[CRICKET CACHE] Save failed: " +
            e
        );
    }
}


/*
 * ESPN team/athlete IDs are only unique inside a league.
 *
 * We add a stable league offset before persisting favorites so two leagues
 * cannot collide with one another.
 *
 * Past folders deliberately share the same offset as their corresponding
 * current folder. A player/team should therefore retain the same favorite
 * identity regardless of whether it is viewed through Current or Past.
 */
const LEAGUE_OFFSETS = {
    "NFL": 1000000,
    "NCAAF": 2000000,
    "UFL": 3000000,
    "CFL": 4000000,

    "MLB": 5000000,
    "NCAA Base": 6000000,
    "WBC": 7000000,

    "NHL": 8000000,
    "NCAA Hockey": 9000000,

    "NBA": 10000000,
    "WNBA": 11000000,
    "NCAAM": 12000000,
    "FIBA": 13000000,

    "MLS": 15000000,
    "EPL": 16000000,
    "La Liga": 17000000,
    "Bundesliga": 18000000,
    "Serie A": 19000000,
    "Liga MX": 20000000,
    "UEFA Champ": 21000000,
    "World Cup": 22000000,

    "NRL": 23000000,
    "Six Nations": 24000000,
    "Rugby WC": 25000000,

    "International": 26000000,
    "Int'l Past": 26000000,

    "IPL": 27000000,
    "IPL Past": 27000000,

    "MLC": 28000000,
    "MLC Past": 28000000,

    "ATP": 29000000,
    "ATP Past": 29000000,

    "WTA": 30000000,
    "WTA Past": 30000000,

    "AFL": 31000000,
    "UFC": 32000000,

    "Women's WC": 33000000
};


let s_timeline_sync_timer = null;


// =========================================================
// PUBLIC GAME REQUEST
// =========================================================

function getGames(
    sport,
    leagueIndex,
    onLoad,
    onError
) {
    if (
        sport ==
        models.sports.FAVORITES
    ) {
        getFavoriteGames(
            storage.storedFavorites(),
            onLoad,
            onError
        );

    } else {
        getGamesForSport(
            sport,
            leagueIndex,
            onLoad,
            onError
        );
    }
}


// =========================================================
// FAVORITE GAMES
// =========================================================

function getFavoriteGames(
    favorites,
    onLoad,
    onError
) {
    /*
     * An empty Favorites list is a valid empty state, not a network error.
     */
    if (
        favorites.length == 0
    ) {
        onLoad([]);
        return;
    }


    /*
     * Group favorites by sport so each sport can be loaded once.
     */
    const sportGroups =
        utils.groupBy(
            favorites,
            favoriteItem =>
                favoriteItem.sport
        );


    const favoriteSports =
        Object.keys(sportGroups)
            .map(
                key =>
                    parseInt(
                        key,
                        10
                    )
            );


    const favoriteGames = [];
    const loadedSports = [];

    let hasError = false;


    Object.values(sportGroups)
        .forEach(
            sportGroup => {

                const sport =
                    sportGroup[0].sport;


                const rawTeamIDs =
                    sportGroup.map(
                        favoriteItem =>
                            favoriteItem.teamID
                    );


                /*
                 * Legacy FIBA migration:
                 *
                 * Older builds used the removed 14,000,000 offset.
                 * Unified FIBA now uses 13,000,000.
                 */
                const teamIDs =
                    rawTeamIDs.map(id => {

                        const parsedId =
                            parseInt(
                                id,
                                10
                            );


                        if (
                            !isNaN(parsedId) &&
                            parsedId >= 14000000 &&
                            parsedId < 15000000
                        ) {
                            return String(
                                parsedId -
                                1000000
                            );
                        }


                        return id;
                    });


                /*
                 * leagueIndex=null requests broad/current coverage.
                 *
                 * getGamesForSport() strips explicit Past endpoints for this
                 * path, so Favorites never fires all the historical requests.
                 */
                getGamesForSport(
                    sport,
                    null,

                    games => {

                        const filtered =
                            games.filter(
                                game =>
                                    teamIDs.includes(
                                        game.team1.id
                                    ) ||
                                    teamIDs.includes(
                                        game.team2.id
                                    )
                            );


                        favoriteGames.push(
                            ...filtered
                        );


                        loadedSports.push(
                            sport
                        );


                        if (
                            favoriteSports.every(
                                s =>
                                    loadedSports.includes(s)
                            )
                        ) {
                            if (
                                favoriteGames.length > 0 ||
                                !hasError
                            ) {
                                updateTimelinePins(
                                    favoriteGames
                                );

                                onLoad(
                                    favoriteGames
                                );

                            } else {
                                onError();
                            }
                        }
                    },


                    () => {

                        hasError = true;

                        loadedSports.push(
                            sport
                        );


                        if (
                            favoriteSports.every(
                                s =>
                                    loadedSports.includes(s)
                            )
                        ) {
                            if (
                                favoriteGames.length > 0
                            ) {
                                updateTimelinePins(
                                    favoriteGames
                                );

                                onLoad(
                                    favoriteGames
                                );

                            } else {
                                onError();
                            }
                        }
                    },


                    /*
                     * Filter Favorites BEFORE applying the watch game cap.
                     */
                    teamIDs
                );
            }
        );
}


// =========================================================
// BREAKDOWN URL HANDLING
// =========================================================

/*
 * Remove only our transient cache-buster from a URL while preserving useful
 * query parameters such as:
 *
 *     dates=YYYYMMDD
 *
 * This prevents Game Info from producing URLs such as:
 *
 *     ?t=old&t=new
 *
 * while still remembering which exact Past-date request originally found
 * the game.
 */
function stripCacheBuster(url) {
    if (
        !url ||
        url.indexOf('?') === -1
    ) {
        return url;
    }


    let parts =
        url.split('?');

    let baseUrl =
        parts.shift();

    let query =
        parts.join('?');


    let params =
        query
            .split('&')
            .filter(param => {
                return (
                    param.split('=')[0] !==
                    't'
                );
            });


    return (
        params.length > 0
            ? baseUrl +
                '?' +
                params.join('&')
            : baseUrl
    );
}


function getSummaryUrl(
    sport,
    leagueName,
    gameId
) {
    /*
     * Tennis, MMA and Cricket are handled from scoreboard-shaped responses.
     *
     * Prefer the exact scoreboard URL that originally produced the game.
     */
    if (
        sport == models.sports.TENNIS ||
        sport == models.sports.MMA ||
        sport == models.sports.CRICKET
    ) {
        if (
            gameScoreboardUrls[gameId]
        ) {
            return (
                gameScoreboardUrls[gameId]
            );
        }
    }


    let endpoints =
        getEndpointsForSport(
            sport
        );


    let target =
        endpoints.find(
            endpoint =>
                endpoint.league ===
                leagueName
        );


    /*
     * Safe fallback for an older/saved game whose exact league label is not
     * available anymore.
     */
    if (
        !target &&
        endpoints.length > 0
    ) {
        target =
            endpoints[0];
    }


    if (!target) {
        return null;
    }


    let baseUrl =
        Array.isArray(target.url)
            ? target.url[0]
            : target.url;


    if (
        sport == models.sports.TENNIS ||
        sport == models.sports.MMA ||
        sport == models.sports.CRICKET
    ) {
        /*
         * Do not recreate a multi-day date range here.
         *
         * A Past game found during this JS session already has the precise
         * single-day URL cached above. This static URL is only the fallback.
         */
        return (
            baseUrl +
            "/scoreboard"
        );
    }


    /*
     * Traditional team sports generally expose richer Game Info through
     * ESPN's /summary endpoint.
     */
    return (
        baseUrl +
        "/summary?event=" +
        gameId
    );
}


// =========================================================
// ESPN GAME BREAKDOWN
// =========================================================

/*
 * Human-friendly label for ESPN linescore arrays.
 */
function breakdownLineScoreLabel(sport) {
    if (
        sport == models.sports.MLB ||
        sport == models.sports.CRICKET
    ) {
        return "Innings";
    }


    if (
        sport ==
        models.sports.TENNIS
    ) {
        return "Sets";
    }


    if (
        sport == models.sports.NFL ||
        sport == models.sports.NBA ||
        sport == models.sports.AFL
    ) {
        return "Quarters";
    }


    if (
        sport ==
        models.sports.MMA
    ) {
        return "Rounds";
    }


    /*
     * NHL, Soccer and Rugby are safely described as periods.
     */
    return "Periods";
}


/*
 * Convert one ESPN linescore object into compact watch-friendly text.
 *
 * Most sports expose displayValue/value directly. Cricket can expose
 * runs/wickets separately, which produces a much more useful innings line.
 */
function formatBreakdownLineScore(
    sport,
    line
) {
    if (!line) {
        return "";
    }


    if (
        sport ==
        models.sports.CRICKET
    ) {
        if (
            line.runs != null
        ) {
            let text =
                String(
                    line.runs
                );


            if (
                line.wickets != null
            ) {
                text +=
                    "/" +
                    line.wickets;
            }


            return text;
        }
    }


    if (
        line.displayValue != null
    ) {
        return String(
            line.displayValue
        );
    }


    if (
        line.value != null
    ) {
        return String(
            line.value
        );
    }


    return "";
}


function getGameBreakdown(
    sport,
    leagueName,
    gameId,
    requestId,
    sendCallback
) {
    let summaryUrl =
        getSummaryUrl(
            sport,
            leagueName,
            gameId
        );


    if (!summaryUrl) {
        sendCallback({
            "LOAD_BREAKDOWN":
                "Unsupported League",

            "REQUEST_ID":
                requestId
        });

        return;
    }


    /*
     * Always add a fresh cache-buster.
     */
    const url =
        summaryUrl +
        (
            summaryUrl.indexOf('?') > -1
                ? '&'
                : '?'
        ) +
        "t=" +
        Date.now();


    console.log(
        "[BREAKDOWN REQUEST] " +
        url
    );


    let req =
        new XMLHttpRequest();


    req.open(
        'GET',
        url
    );


    req.onload =
        function() {

            if (
                req.status === 200
            ) {
                try {
                    let breakdownText =
                        "ESPN no longer provides breakdown data for this past matchup, or data is currently unavailable.";


                    let data =
                        JSON.parse(
                            req.responseText
                        );


                    let comps = null;
                    let situation = null;


                    /*
                     * Traditional /summary responses.
                     */
                    if (
                        data.header &&
                        data.header.competitions &&
                        data.header.competitions.length > 0
                    ) {
                        comps =
                            data.header
                                .competitions[0]
                                .competitors;


                        situation =
                            data.header
                                .competitions[0]
                                .situation;


                    /*
                     * Scoreboard responses.
                     *
                     * Most sports:
                     *
                     *     event.competitions[]
                     *
                     * Tennis frequently:
                     *
                     *     event.groupings[].competitions[]
                     *
                     * This mirrors the main Tennis parser so Game Info can
                     * locate the exact flattened match ID.
                     */
                    } else if (
                        data.events
                    ) {
                        let targetComp =
                            null;


                        for (
                            let eventIndex = 0;
                            eventIndex < data.events.length;
                            eventIndex++
                        ) {
                            const event =
                                data.events[eventIndex];


                            /*
                             * First search ordinary competitions.
                             */
                            if (
                                event.competitions &&
                                event.competitions.length > 0
                            ) {
                                targetComp =
                                    event.competitions.find(
                                        competition =>
                                            String(
                                                competition.id
                                            ) ===
                                            String(
                                                gameId
                                            )
                                    ) ||
                                    null;


                                if (
                                    !targetComp &&
                                    String(event.id) ===
                                        String(gameId)
                                ) {
                                    targetComp =
                                        event.competitions[0];
                                }
                            }


                            if (targetComp) {
                                break;
                            }


                            /*
                             * Tennis commonly nests individual matches inside
                             * groupings.
                             */
                            if (
                                event.groupings
                            ) {
                                for (
                                    let groupIndex = 0;
                                    groupIndex < event.groupings.length;
                                    groupIndex++
                                ) {
                                    const grouping =
                                        event.groupings[
                                            groupIndex
                                        ];


                                    if (
                                        !grouping.competitions ||
                                        grouping.competitions.length === 0
                                    ) {
                                        continue;
                                    }


                                    targetComp =
                                        grouping.competitions.find(
                                            competition =>
                                                String(
                                                    competition.id
                                                ) ===
                                                String(
                                                    gameId
                                                )
                                        ) ||
                                        null;


                                    if (targetComp) {
                                        break;
                                    }
                                }
                            }


                            if (targetComp) {
                                break;
                            }
                        }


                        if (targetComp) {
                            comps =
                                targetComp.competitors;

                            situation =
                                targetComp.situation;

                        } else {
                            console.log(
                                "[BREAKDOWN MATCH NOT FOUND] sport=" +
                                sport +
                                " gameId=" +
                                gameId +
                                " events=" +
                                data.events.length
                            );
                        }
                    }


                    if (
                        comps &&
                        comps.length >= 2
                    ) {
                        let t1Obj =
                            comps[1].team ||
                            comps[1].athlete ||
                            {};


                        let t2Obj =
                            comps[0].team ||
                            comps[0].athlete ||
                            {};


                        let t1 =
                            String(
                                t1Obj.abbreviation ||
                                t1Obj.shortName ||
                                t1Obj.lastName ||
                                "TM1"
                            ).substring(
                                0,
                                5
                            );


                        let t2 =
                            String(
                                t2Obj.abbreviation ||
                                t2Obj.shortName ||
                                t2Obj.lastName ||
                                "TM2"
                            ).substring(
                                0,
                                5
                            );


                        let s1 =
                            comps[1].score != null
                                ? comps[1].score
                                : "0";


                        let s2 =
                            comps[0].score != null
                                ? comps[0].score
                                : "0";


                        breakdownText =
                            t1 +
                            "   " +
                            s1 +
                            " - " +
                            s2 +
                            "   " +
                            t2;


                        let sitText =
                            gameDetails(
                                sport,
                                situation
                            );


                        if (
                            sitText &&
                            sitText.trim() !== ""
                        ) {
                            breakdownText +=
                                "\n\n" +
                                sitText;
                        }


                        /*
                         * Add period/set/inning scores when ESPN exposes them.
                         */
                        if (
                            comps[1].linescores &&
                            comps[0].linescores &&
                            comps[1].linescores.length > 0 &&
                            comps[0].linescores.length > 0
                        ) {
                            let sets1 =
                                comps[1]
                                    .linescores
                                    .map(
                                        line =>
                                            formatBreakdownLineScore(
                                                sport,
                                                line
                                            )
                                    )
                                    .filter(
                                        value =>
                                            value !== ""
                                    )
                                    .join("  ");


                            let sets2 =
                                comps[0]
                                    .linescores
                                    .map(
                                        line =>
                                            formatBreakdownLineScore(
                                                sport,
                                                line
                                            )
                                    )
                                    .filter(
                                        value =>
                                            value !== ""
                                    )
                                    .join("  ");


                            if (
                                sets1 !== "" ||
                                sets2 !== ""
                            ) {
                                let label =
                                    breakdownLineScoreLabel(
                                        sport
                                    );


                                breakdownText +=
                                    "\n\n" +
                                    label +
                                    ":\n" +
                                    t1 +
                                    ":  " +
                                    sets1 +
                                    "\n" +
                                    t2 +
                                    ":  " +
                                    sets2;
                            }
                        }
                    }


                    /*
                     * Keep Game Info comfortably inside the watch AppMessage
                     * inbox.
                     */
                    if (
                        breakdownText.length >
                        450
                    ) {
                        breakdownText =
                            breakdownText.substring(
                                0,
                                447
                            ) +
                            "...";
                    }


                    sendCallback({
                        "LOAD_BREAKDOWN":
                            breakdownText,

                        "REQUEST_ID":
                            requestId
                    });


                } catch (e) {
                    console.log(
                        "[BREAKDOWN PARSE ERROR] " +
                        e
                    );


                    sendCallback({
                        "LOAD_BREAKDOWN":
                            "Parse Error",

                        "REQUEST_ID":
                            requestId
                    });
                }


            } else {
                console.log(
                    "[BREAKDOWN HTTP ERROR] Status: " +
                    req.status
                );


                sendCallback({
                    "LOAD_BREAKDOWN":
                        "ESPN no longer provides breakdown data for this past matchup, or data is currently unavailable.",

                    "REQUEST_ID":
                        requestId
                });
            }
        };


    req.onerror =
        function() {
            sendCallback({
                "LOAD_BREAKDOWN":
                    "Connection Error",

                "REQUEST_ID":
                    requestId
            });
        };


    req.send();
}


// =========================================================
// ESPN ENDPOINTS
// =========================================================

/*
 * IMPORTANT:
 *
 * Keep this array order synchronized with league-menu.c.
 *
 * The watch sends only a numeric league index, so changing the order here
 * without changing the corresponding C menu would silently request the wrong
 * ESPN competition.
 */
function getEndpointsForSport(
    sport
) {
    var base =
        "https://site.api.espn.com/apis/site/v2/sports";


    switch (sport) {

        // -------------------------------------------------
        // AMERICAN FOOTBALL
        // -------------------------------------------------

        case models.sports.NFL:
            return [
                {
                    url:
                        base +
                        '/football/nfl',

                    league:
                        "NFL"
                },

                {
                    url:
                        base +
                        '/football/college-football',

                    league:
                        "NCAAF",

                    params:
                        "groups=80"
                },

                {
                    url:
                        base +
                        '/football/ufl',

                    league:
                        "UFL"
                },

                {
                    url:
                        base +
                        '/football/cfl',

                    league:
                        "CFL"
                }
            ];


        // -------------------------------------------------
        // BASEBALL
        // -------------------------------------------------

        case models.sports.MLB:
            return [
                {
                    url:
                        base +
                        '/baseball/mlb',

                    league:
                        "MLB"
                },

                {
                    url:
                        base +
                        '/baseball/college-baseball',

                    league:
                        "NCAA Base"
                },

                {
                    url:
                        base +
                        '/baseball/world-baseball-classic',

                    league:
                        "WBC"
                }
            ];


        // -------------------------------------------------
        // HOCKEY
        // -------------------------------------------------

        case models.sports.NHL:
            return [
                {
                    url:
                        base +
                        '/hockey/nhl',

                    league:
                        "NHL"
                },

                {
                    url:
                        base +
                        '/hockey/mens-college-hockey',

                    league:
                        "NCAA Hockey"
                }
            ];


        // -------------------------------------------------
        // BASKETBALL
        // -------------------------------------------------

        case models.sports.NBA:
            return [
                {
                    url:
                        base +
                        '/basketball/nba',

                    league:
                        "NBA"
                },

                {
                    url:
                        base +
                        '/basketball/wnba',

                    league:
                        "WNBA"
                },

                {
                    url:
                        base +
                        '/basketball/mens-college-basketball',

                    league:
                        "NCAAM",

                    params:
                        "groups=50"
                },

                {
                    url:
                        base +
                        '/basketball/fiba',

                    league:
                        "FIBA"
                }
            ];


        // -------------------------------------------------
        // SOCCER
        // -------------------------------------------------

        case models.sports.MLS:
            return [
                {
                    url:
                        base +
                        '/soccer/usa.1',

                    league:
                        "MLS"
                },

                {
                    url:
                        base +
                        '/soccer/eng.1',

                    league:
                        "EPL"
                },

                {
                    url:
                        base +
                        '/soccer/esp.1',

                    league:
                        "La Liga"
                },

                {
                    url:
                        base +
                        '/soccer/ger.1',

                    league:
                        "Bundesliga"
                },

                {
                    url:
                        base +
                        '/soccer/ita.1',

                    league:
                        "Serie A"
                },

                {
                    url:
                        base +
                        '/soccer/mex.1',

                    league:
                        "Liga MX"
                },

                {
                    url:
                        base +
                        '/soccer/uefa.champions',

                    league:
                        "UEFA Champ"
                },

                {
                    url:
                        base +
                        '/soccer/fifa.world',

                    league:
                        "World Cup"
                },

                {
                    url:
                        base +
                        '/soccer/fifa.wwc',

                    league:
                        "Women's WC"
                }
            ];


        // -------------------------------------------------
        // RUGBY
        // -------------------------------------------------

        case models.sports.RUGBY:
            return [
                {
                    url:
                        base +
                        '/rugby-league/3',

                    league:
                        "NRL"
                },

                {
                    url:
                        base +
                        '/rugby/180659',

                    league:
                        "Six Nations"
                },

                {
                    url:
                        base +
                        '/rugby/164205',

                    league:
                        "Rugby WC"
                }
            ];


        // -------------------------------------------------
        // CRICKET
        // -------------------------------------------------

        case models.sports.CRICKET:
            return [
                /*
                 * 8039/8040 remain as lightweight fallback feeds.
                 *
                 * Dynamic header discovery is the primary source for modern
                 * active International Cricket series.
                 */
                {
                    url: [
                        base +
                            '/cricket/8039',

                        base +
                            '/cricket/8040'
                    ],

                    league:
                        "International",

                    isPast:
                        false
                },

                /*
                 * The URLs remain here for fallback/Game Info routing.
                 *
                 * getGamesForSport() deliberately skips daily requests against
                 * these legacy feeds for Int'l Past and instead uses dynamic
                 * numeric series discovery.
                 */
                {
                    url: [
                        base +
                            '/cricket/8039',

                        base +
                            '/cricket/8040'
                    ],

                    league:
                        "Int'l Past",

                    isPast:
                        true
                },

                {
                    url:
                        base +
                        '/cricket/8048',

                    league:
                        "IPL",

                    isPast:
                        false
                },

                {
                    url:
                        base +
                        '/cricket/8048',

                    league:
                        "IPL Past",

                    isPast:
                        true
                },

                {
                    url:
                        base +
                        '/cricket/1528556',

                    league:
                        "MLC",

                    isPast:
                        false
                },

                {
                    url:
                        base +
                        '/cricket/1528556',

                    league:
                        "MLC Past",

                    isPast:
                        true
                }
            ];


        // -------------------------------------------------
        // TENNIS
        // -------------------------------------------------

        case models.sports.TENNIS:
            return [
                {
                    url:
                        base +
                        '/tennis/atp',

                    league:
                        "ATP",

                    isPast:
                        false
                },

                {
                    url:
                        base +
                        '/tennis/atp',

                    league:
                        "ATP Past",

                    isPast:
                        true
                },

                {
                    url:
                        base +
                        '/tennis/wta',

                    league:
                        "WTA",

                    isPast:
                        false
                },

                {
                    url:
                        base +
                        '/tennis/wta',

                    league:
                        "WTA Past",

                    isPast:
                        true
                }
            ];


        // -------------------------------------------------
        // AFL
        // -------------------------------------------------

        case models.sports.AFL:
            return [
                {
                    url:
                        base +
                        '/australian-football/afl',

                    league:
                        "AFL"
                }
            ];


        // -------------------------------------------------
        // MMA
        // -------------------------------------------------

        case models.sports.MMA:
            return [
                {
                    url:
                        base +
                        '/mma/ufc',

                    league:
                        "UFC"
                }
            ];


        default:
            return [];
    }
}


// =========================================================
// FETCH SPORTS GAMES
// =========================================================

/*
 * Main scoreboard loader.
 *
 * leagueIndex:
 *
 * - A numeric value means the user selected a particular league/folder.
 * - null/undefined means this is a broad request, such as Favorites or a
 *   background game refresh.
 *
 * Broad requests deliberately exclude all explicit Past folders.
 */
function getGamesForSport(
    sport,
    leagueIndex,
    onLoad,
    onError,
    favoriteTeamIDs
) {
    if (DEBUG_MOCK) {
        onLoad(
            mock.nfl
        );

        return;
    }


    let endpoints =
        getEndpointsForSport(
            sport
        );


    let allGames = [];

    let completedRequests = 0;
    let hasCriticalError = false;
    let hasLoaded = false;


    if (
        endpoints.length === 0
    ) {
        onError();
        return;
    }


    let isPastReq = false;


    /*
     * A valid league index means the watch intentionally selected exactly one
     * folder.
     */
    if (
        leagueIndex !== undefined &&
        leagueIndex !== null &&
        leagueIndex >= 0 &&
        leagueIndex < endpoints.length
    ) {
        endpoints = [
            endpoints[leagueIndex]
        ];

        isPastReq =
            endpoints[0].isPast ===
            true;


    } else {
        /*
         * Favorites/global requests should never fan out into historical
         * folders.
         */
        endpoints =
            endpoints.filter(
                endpoint =>
                    endpoint.isPast !==
                    true
            );
    }


    /*
     * International Cricket Past is different from IPL/MLC Past.
     *
     * Recent International tours live in dynamic numeric ESPN series IDs,
     * not reliably inside the old 8039/8040 buckets.
     */
    const isCricketInternationalPast =
        sport ==
            models.sports.CRICKET &&
        leagueIndex == 1 &&
        isPastReq;


    let fetchTasks = [];


    endpoints.forEach(
        endpoint => {

            let urls =
                Array.isArray(
                    endpoint.url
                )
                    ? endpoint.url
                    : [
                        endpoint.url
                    ];


            urls.forEach(
                baseUrl => {

                    let baseParams =
                        endpoint.params ||
                        "";


                    if (
                        endpoint.isPast &&
                        isCricketInternationalPast
                    ) {
                        /*
                         * Skip the wasteful 8039/8040 x 13-day request set.
                         *
                         * Dynamic series tasks are added after Cricket header
                         * discovery below.
                         */
                        return;


                    } else if (
                        endpoint.isPast
                    ) {
                        /*
                         * Past Tennis / IPL / MLC:
                         *
                         * Use individual dates rather than a YYYYMMDD-YYYYMMDD
                         * range because ESPN's range support is inconsistent.
                         *
                         * Current owns roughly the newest 48 hours. Past owns
                         * days 2 through 14.
                         */
                        for (
                            let dayOffset = 2;
                            dayOffset <= 14;
                            dayOffset++
                        ) {
                            let day =
                                new Date();


                            day.setDate(
                                day.getDate() -
                                dayOffset
                            );


                            let dayString =
                                day
                                    .toISOString()
                                    .slice(
                                        0,
                                        10
                                    )
                                    .replace(
                                        /-/g,
                                        ''
                                    );


                            let params =
                                baseParams;


                            params +=
                                (
                                    params === ""
                                        ? ""
                                        : "&"
                                ) +
                                "dates=" +
                                dayString;


                            fetchTasks.push({
                                url:
                                    baseUrl,

                                league:
                                    endpoint.league,

                                params:
                                    params
                            });
                        }


                    } else {
                        fetchTasks.push({
                            url:
                                baseUrl,

                            league:
                                endpoint.league,

                            params:
                                baseParams
                        });
                    }
                }
            );
        }
    );


    function finishSuccess(
        games
    ) {
        if (hasLoaded) {
            return;
        }

        hasLoaded = true;

        onLoad(
            games
        );
    }


    function finishError() {
        if (hasLoaded) {
            return;
        }

        hasLoaded = true;

        onError();
    }


    // -----------------------------------------------------
    // BOUNDED REQUEST QUEUE
    // -----------------------------------------------------

    function executeFetchTasks() {
        let activeRequests = 0;
        let taskIndex = 0;

        const MAX_CONCURRENT = 4;


        function runNext() {
            if (
                taskIndex >=
                fetchTasks.length
            ) {
                return;
            }


            let task =
                fetchTasks[
                    taskIndex++
                ];


            activeRequests++;


            let req =
                new XMLHttpRequest();


            let cleanParams =
                task.params &&
                task.params.startsWith('?')
                    ? task.params.substring(1)
                    : (
                        task.params ||
                        ""
                    );


            let paramString =
                cleanParams
                    ? "&" +
                        cleanParams
                    : "";


            const fullUrl =
                task.url +
                "/scoreboard?t=" +
                Date.now() +
                paramString;


            console.log(
                "[ESPN REQUEST] " +
                fullUrl
            );


            req.open(
                'GET',
                fullUrl
            );


            let isTimeout = false;


            let watchdog =
                setTimeout(
                    function() {
                        isTimeout = true;
                        hasCriticalError = true;

                        completedRequests++;
                        activeRequests--;


                        try {
                            req.abort();

                        } catch (e) {}


                        checkCompletion();
                        runNext();
                    },

                    10000
                );


            req.onload =
                function() {

                    if (isTimeout) {
                        return;
                    }


                    clearTimeout(
                        watchdog
                    );


                    if (
                        req.readyState !=
                        4
                    ) {
                        return;
                    }


                    console.log(
                        "[ESPN RESPONSE] " +
                        req.status +
                        " " +
                        task.league
                    );


                    if (
                        req.status ==
                        200
                    ) {
                        try {
                            let sportsData =
                                JSON.parse(
                                    req.responseText
                                );


                            if (
                                sportsData.events
                            ) {
                                let allParsedEvents = [];


                                /*
                                 * Flatten each real ESPN competition into one
                                 * pseudo-event.
                                 *
                                 * Tennis can put matches under groupings,
                                 * whereas most team sports use competitions
                                 * directly.
                                 */
                                const processCompetition =
                                    (
                                        comp,
                                        event
                                    ) => {

                                        let status =
                                            comp.status &&
                                            comp.status.type
                                                ? comp.status
                                                    .type
                                                    .name
                                                : "";


                                        let shortDetail =
                                            comp.status &&
                                            comp.status.type
                                                ? (
                                                    comp.status
                                                        .type
                                                        .shortDetail ||
                                                    ""
                                                )
                                                : "";


                                        let p1 =
                                            comp.competitors &&
                                            comp.competitors.length > 1
                                                ? (
                                                    comp.competitors[1]
                                                        .athlete ||
                                                    comp.competitors[1]
                                                        .team
                                                )
                                                : null;


                                        let p2 =
                                            comp.competitors &&
                                            comp.competitors.length > 0
                                                ? (
                                                    comp.competitors[0]
                                                        .athlete ||
                                                    comp.competitors[0]
                                                        .team
                                                )
                                                : null;


                                        let name1 =
                                            p1
                                                ? (
                                                    p1.displayName ||
                                                    p1.shortName ||
                                                    "TBD"
                                                )
                                                : "TBD";


                                        let name2 =
                                            p2
                                                ? (
                                                    p2.displayName ||
                                                    p2.shortName ||
                                                    "TBD"
                                                )
                                                : "TBD";


                                        /*
                                         * Retired/walkover Tennis entries
                                         * should not appear as active games.
                                         */
                                        if (
                                            status ===
                                                "STATUS_RETIRED" ||
                                            status ===
                                                "STATUS_WALKOVER"
                                        ) {
                                            return;
                                        }


                                        if (
                                            shortDetail.indexOf(
                                                "Retired"
                                            ) !== -1 ||
                                            shortDetail.indexOf(
                                                "Walkover"
                                            ) !== -1
                                        ) {
                                            return;
                                        }


                                        /*
                                         * Completely unresolved future
                                         * brackets are not useful on-watch.
                                         */
                                        if (
                                            name1 ===
                                                "TBD" &&
                                            name2 ===
                                                "TBD"
                                        ) {
                                            return;
                                        }


                                        allParsedEvents.push({
                                            id:
                                                comp.id ||
                                                event.id,

                                            eventId:
                                                event.id,

                                            name:
                                                event.name,

                                            competitions: [
                                                comp
                                            ]
                                        });
                                    };


                                sportsData.events
                                    .forEach(
                                        event => {

                                            if (
                                                event.competitions
                                            ) {
                                                event.competitions
                                                    .forEach(
                                                        comp =>
                                                            processCompetition(
                                                                comp,
                                                                event
                                                            )
                                                    );


                                            } else if (
                                                event.groupings
                                            ) {
                                                event.groupings
                                                    .forEach(
                                                        grouping => {

                                                            if (
                                                                grouping.competitions
                                                            ) {
                                                                grouping
                                                                    .competitions
                                                                    .forEach(
                                                                        comp =>
                                                                            processCompetition(
                                                                                comp,
                                                                                event
                                                                            )
                                                                    );
                                                            }
                                                        }
                                                    );
                                            }
                                        }
                                    );


                                let games =
                                    allParsedEvents
                                        .map(
                                            event =>
                                                parseEvent(
                                                    sport,
                                                    task.league,
                                                    event,
                                                    fullUrl
                                                )
                                        )
                                        .filter(
                                            game =>
                                                game !==
                                                null
                                        );


                                allGames =
                                    allGames.concat(
                                        games
                                    );
                            }


                        } catch (e) {
                            console.log(
                                "[ESPN JSON ERROR] " +
                                fullUrl +
                                " :: " +
                                e
                            );


                            hasCriticalError =
                                true;
                        }


                    } else if (
                        req.status != 404
                    ) {
                        /*
                         * A missing/off-season endpoint is allowed to be empty.
                         *
                         * Other HTTP failures count as a real network/server
                         * failure if no usable request succeeds.
                         */
                        hasCriticalError =
                            true;
                    }


                    completedRequests++;
                    activeRequests--;


                    checkCompletion();
                    runNext();
                };


            req.onerror =
                function() {

                    if (isTimeout) {
                        return;
                    }


                    clearTimeout(
                        watchdog
                    );


                    hasCriticalError =
                        true;

                    completedRequests++;
                    activeRequests--;


                    checkCompletion();
                    runNext();
                };


            req.send();
        }


        /*
         * Seed the queue with at most four requests.
         *
         * Each completion starts one more queued request.
         */
        for (
            let i = 0;
            i < MAX_CONCURRENT &&
            i < fetchTasks.length;
            i++
        ) {
            runNext();
        }
    }


    // -----------------------------------------------------
    // COMPLETION / DEDUPE / SORT / DATE FILTER / WATCH CAP
    // -----------------------------------------------------

    function checkCompletion() {

        if (
            completedRequests !==
            fetchTasks.length
        ) {
            return;
        }


        if (hasLoaded) {
            return;
        }


        console.log(
            "[ESPN COMPLETE] Requests: " +
            completedRequests +
            " Games: " +
            allGames.length
        );


        if (
            allGames.length > 0
        ) {
            const seenIds =
                new Set();


            /*
             * Dynamic discovery can find the same matchup through more than
             * one route.
             */
            const uniqueGames =
                allGames.filter(
                    game => {

                        if (
                            seenIds.has(
                                game.id
                            )
                        ) {
                            return false;
                        }


                        seenIds.add(
                            game.id
                        );


                        return true;
                    }
                );


            /*
             * LIVE games float to the top in Current folders.
             *
             * Past folders are ultimately filtered to completed games only,
             * but this weight remains harmless before filtering.
             *
             * Current:
             *     chronological ascending.
             *
             * Past:
             *     chronological descending so the watch cap preserves the
             *     newest completed games.
             */
            uniqueGames.sort(
                (a, b) => {

                    const getWeight =
                        game => {

                            const details =
                                String(
                                    game.details ||
                                    ""
                                )
                                    .trim()
                                    .toUpperCase();


                            return (
                                details ===
                                "LIVE"
                            )
                                ? 0
                                : 1;
                        };


                    const weightA =
                        getWeight(a);

                    const weightB =
                        getWeight(b);


                    if (
                        weightA !==
                        weightB
                    ) {
                        return (
                            weightA -
                            weightB
                        );
                    }


                    if (
                        a.startTime &&
                        b.startTime
                    ) {
                        const timeA =
                            a.startTime
                                .getTime();

                        const timeB =
                            b.startTime
                                .getTime();


                        if (
                            !isNaN(timeA) &&
                            !isNaN(timeB)
                        ) {
                            return (
                                isPastReq
                                    ? (
                                        timeB -
                                        timeA
                                    )
                                    : (
                                        timeA -
                                        timeB
                                    )
                            );
                        }
                    }


                    return 0;
                }
            );


            const nowMs =
                Date.now();

            const DAY_MS =
                24 *
                60 *
                60 *
                1000;


            let filteredGames =
                uniqueGames.filter(
                    game => {

                        /*
                         * LIVE / PAST FOLDER HANDLING
                         * ---------------------------
                         *
                         * ESPN Tennis scoreboards sometimes return current or
                         * live competitions even when an older `dates=` value
                         * was requested.
                         *
                         * Therefore a Past folder must reject a live game
                         * before any LIVE override is considered.
                         *
                         * Current folders still retain genuinely live games
                         * regardless of their original start timestamp. This
                         * is essential for multi-day Cricket Test matches.
                         */
                        const isLive =
                            String(
                                game.details ||
                                ""
                            )
                                .trim()
                                .toUpperCase() ===
                            "LIVE";


                        if (
                            !game.startTime ||
                            isNaN(
                                game.startTime
                                    .getTime()
                            )
                        ) {
                            /*
                             * A Past folder cannot safely classify an event
                             * without a usable start time, so reject it.
                             *
                             * Current folders remain permissive so an
                             * otherwise valid ESPN matchup is not silently
                             * lost.
                             */
                            return isPastReq
                                ? false
                                : true;
                        }


                        const gameTime =
                            game.startTime
                                .getTime();


                        /*
                         * EXPLICIT PAST FOLDERS
                         * ---------------------
                         *
                         * Past means completed historical games only.
                         *
                         * Never allow a current/live match to bleed into:
                         *
                         * - ATP Past
                         * - WTA Past
                         * - Int'l Past
                         * - IPL Past
                         * - MLC Past
                         *
                         * Keep games from the previous 14 days while leaving
                         * roughly the newest 48 hours to Current.
                         */
                        if (isPastReq) {

                            if (isLive) {
                                return false;
                            }


                            const pastLowerBound =
                                nowMs -
                                14 *
                                DAY_MS;


                            const pastUpperBound =
                                nowMs -
                                2 *
                                DAY_MS;


                            return (
                                gameTime >=
                                    pastLowerBound &&
                                gameTime <
                                    pastUpperBound
                            );
                        }


                        /*
                         * Only Current folders reach this point.
                         *
                         * Never discard a genuinely active game just because
                         * its original start timestamp falls outside the
                         * normal retrospective window.
                         */
                        if (isLive) {
                            return true;
                        }


                        /*
                         * CURRENT FOLDER LOOKBACK POLICY
                         * ------------------------------
                         *
                         * Most sports:
                         *     1 day
                         *
                         * Tennis:
                         *     2 days
                         *     (older matches have dedicated Past folders)
                         *
                         * Cricket:
                         *     2 days for completed games.
                         *     Older active Tests survive through LIVE override.
                         *
                         * Rugby:
                         *     7 days
                         *     (sparse schedule and no Past folders)
                         */
                        let lookbackDays =
                            1;


                        if (
                            game.sport ==
                            models.sports.TENNIS
                        ) {
                            lookbackDays =
                                2;


                        } else if (
                            game.sport ==
                            models.sports.CRICKET
                        ) {
                            lookbackDays =
                                2;


                        } else if (
                            game.sport ==
                            models.sports.RUGBY
                        ) {
                            lookbackDays =
                                7;
                        }


                        const lowerBound =
                            nowMs -
                            lookbackDays *
                            DAY_MS;


                        /*
                         * Tennis receives a smaller forward window because
                         * ESPN can expose extremely large future brackets.
                         */
                        const futureLookahead =
                            game.sport ==
                            models.sports.TENNIS
                                ? 7 *
                                    DAY_MS
                                : 14 *
                                    DAY_MS;


                        const upperBound =
                            nowMs +
                            futureLookahead;


                        return (
                            gameTime >=
                                lowerBound &&
                            gameTime <=
                                upperBound
                        );
                    }
                );


            /*
             * FAVORITES BEFORE CAP
             * --------------------
             *
             * Filter to the user's teams BEFORE reducing the payload for the
             * watch.
             */
            if (
                favoriteTeamIDs &&
                favoriteTeamIDs.length > 0
            ) {
                filteredGames =
                    filteredGames.filter(
                        game =>
                            favoriteTeamIDs.includes(
                                game.team1.id
                            ) ||
                            favoriteTeamIDs.includes(
                                game.team2.id
                            )
                    );
            }


            /*
             * WATCH-SPECIFIC GAME CAPS
             */
            let maxGames =
                MAX_GAMES_DEFAULT;


            if (
                typeof Pebble !==
                    'undefined' &&
                typeof Pebble
                    .getActiveWatchInfo ===
                    'function'
            ) {
                let watchInfo =
                    Pebble
                        .getActiveWatchInfo();


                if (watchInfo) {
                    const platform =
                        String(
                            watchInfo.platform ||
                            ""
                        )
                            .toLowerCase();


                    if (
                        platform ===
                        "aplite"
                    ) {
                        maxGames =
                            MAX_GAMES_APLITE;


                    } else if (
                        platform ===
                            "emery" ||
                        platform ===
                            "gabbro"
                    ) {
                        maxGames =
                            MAX_GAMES_HIGH_MEMORY;
                    }
                }
            }


            if (
                filteredGames.length >
                maxGames
            ) {
                console.log(
                    "Capping payload at " +
                    maxGames +
                    " games."
                );


                filteredGames =
                    filteredGames.slice(
                        0,
                        maxGames
                    );
            }


            console.log(
                "[ESPN SUCCESS] Returning " +
                filteredGames.length +
                " games."
            );


            finishSuccess(
                filteredGames
            );


        } else if (
            hasCriticalError
        ) {
            finishError();


        } else {
            /*
             * No games is a successful empty response.
             */
            finishSuccess([]);
        }
    }


    // -----------------------------------------------------
    // DYNAMIC ESPN LEAGUE DISCOVERY
    // -----------------------------------------------------

    /*
     * Add recently discovered Cricket series to the request queue.
     *
     * Current:
     *     used as a fallback after header failure.
     *
     * Int'l Past:
     *     used both after successful discovery and after header failure, so
     *     recently-ended series remain available.
     *
     * A cached series is queried ONCE, not once per historical day.
     *
     * limit=100 encourages the series scoreboard to expose a larger nearby
     * schedule/result set. The local 2-14-day filter then chooses Past games.
     */
    function addCachedCricketSeriesTasks(
        forPastFolder
    ) {
        if (
            sport !=
            models.sports.CRICKET
        ) {
            return 0;
        }


        const cachedSeries =
            loadCricketSeriesCache();


        let added =
            0;


        cachedSeries.forEach(
            entry => {

                const dynamicUrl =
                    "https://site.api.espn.com/apis/site/v2/sports/cricket/" +
                    encodeURIComponent(
                        String(
                            entry.id
                        )
                    );


                if (
                    fetchTasks.some(
                        task =>
                            task.url ===
                            dynamicUrl
                    )
                ) {
                    return;
                }


                fetchTasks.push({
                    url:
                        dynamicUrl,

                    league:
                        entry.league ||
                        (
                            forPastFolder
                                ? "Int'l Past"
                                : "International"
                        ),

                    params:
                        forPastFolder
                            ? "limit=100"
                            : ""
                });


                added++;
            }
        );


        if (
            added > 0
        ) {
            console.log(
                "[CRICKET CACHE] Added " +
                added +
                " cached series tasks."
            );
        }


        return added;
    }


    /*
     * Normal broad loads may use discovery.
     *
     * Cricket explicitly enables discovery for:
     *
     *     index 0 = International
     *     index 1 = Int'l Past
     */
    let allowDiscovery =
        leagueIndex == null ||
        (
            sport ==
                models.sports.CRICKET &&
            (
                leagueIndex == 0 ||
                leagueIndex == 1
            )
        );


    if (allowDiscovery) {
        let sportString =
            "";


        switch (sport) {

            case models.sports.MLB:
                sportString =
                    "baseball";
                break;


            case models.sports.NHL:
                sportString =
                    "hockey";
                break;


            case models.sports.MLS:
                sportString =
                    "soccer";
                break;


            case models.sports.RUGBY:
                sportString =
                    "rugby";
                break;


            case models.sports.CRICKET:
                sportString =
                    "cricket";
                break;


            case models.sports.TENNIS:
                sportString =
                    "tennis";
                break;


            default:
                sportString =
                    "";
                break;
        }


        if (
            sportString !== ""
        ) {
            let headerRetryCount =
                0;


            /*
             * Start the normal scoreboard queue after discovery.
             *
             * useCricketCache=true means discovery itself failed and we
             * should reuse previous numeric Cricket series IDs.
             *
             * There is deliberately NO generic
             * /sports/cricket/scoreboard fallback here.
             */
            const finishHeaderDiscovery =
                function(
                    useCricketCache
                ) {

                    if (
                        useCricketCache &&
                        sport ==
                        models.sports.CRICKET
                    ) {
                        addCachedCricketSeriesTasks(
                            isCricketInternationalPast
                        );
                    }


                    if (
                        fetchTasks.length ===
                        0
                    ) {
                        checkCompletion();

                    } else {
                        executeFetchTasks();
                    }
                };


            const fetchHeader =
                function() {

                    let headerReq =
                        new XMLHttpRequest();


                    let headerUrl =
                        'https://site.api.espn.com/apis/personalized/v2/scoreboard/header?sport=' +
                        encodeURIComponent(
                            sportString
                        ) +
                        '&t=' +
                        Date.now();


                    console.log(
                        "[ESPN HEADER REQUEST] " +
                        headerUrl
                    );


                    headerReq.open(
                        'GET',
                        headerUrl
                    );


                    let isHeaderTimeout =
                        false;


                    let headerWatchdog =
                        setTimeout(
                            function() {

                                isHeaderTimeout =
                                    true;


                                try {
                                    headerReq.abort();

                                } catch (e) {}


                                if (
                                    headerRetryCount <
                                    1
                                ) {
                                    headerRetryCount++;


                                    console.log(
                                        "[ESPN HEADER RETRY] timeout sport=" +
                                        sportString
                                    );


                                    setTimeout(
                                        fetchHeader,
                                        1000
                                    );


                                    return;
                                }


                                console.log(
                                    "[ESPN DISCOVERY FALLBACK] timeout sport=" +
                                    sportString +
                                    " tasks=" +
                                    fetchTasks.length
                                );


                                finishHeaderDiscovery(
                                    true
                                );
                            },

                            10000
                        );


                    headerReq.onload =
                        function() {

                            if (
                                isHeaderTimeout
                            ) {
                                return;
                            }


                            clearTimeout(
                                headerWatchdog
                            );


                            if (
                                headerReq.readyState !=
                                4
                            ) {
                                return;
                            }


                            console.log(
                                "[ESPN HEADER RESPONSE] " +
                                headerReq.status +
                                " sport=" +
                                sportString
                            );


                            if (
                                headerReq.status ==
                                200
                            ) {
                                try {
                                    let headerData =
                                        JSON.parse(
                                            headerReq
                                                .responseText
                                        );


                                    if (
                                        headerData.sports &&
                                        headerData
                                            .sports
                                            .length >
                                            0
                                    ) {
                                        let activeLeagues =
                                            headerData
                                                .sports[0]
                                                .leagues ||
                                            [];


                                        console.log(
                                            "[ESPN DISCOVERY] " +
                                            sportString +
                                            " active leagues=" +
                                            activeLeagues.length
                                        );


                                        if (
                                            sport ==
                                            models.sports.CRICKET
                                        ) {
                                            saveCricketSeriesCache(
                                                activeLeagues
                                            );


                                            /*
                                             * For Past, use the merged
                                             * current + recently-seen cache.
                                             */
                                            if (
                                                isCricketInternationalPast
                                            ) {
                                                addCachedCricketSeriesTasks(
                                                    true
                                                );
                                            }
                                        }


                                        activeLeagues
                                            .forEach(
                                                league => {

                                                    if (
                                                        !league.id
                                                    ) {
                                                        return;
                                                    }


                                                    let leagueIdentifier =
                                                        sport ==
                                                        models
                                                            .sports
                                                            .CRICKET
                                                            ? league.id
                                                            : (
                                                                league.slug ||
                                                                league
                                                                    .abbreviation ||
                                                                league.id
                                                            );


                                                    let dynamicUrl =
                                                        "https://site.api.espn.com/apis/site/v2/sports/" +
                                                        encodeURIComponent(
                                                            sportString
                                                        ) +
                                                        "/" +
                                                        encodeURIComponent(
                                                            String(
                                                                leagueIdentifier
                                                            )
                                                                .toLowerCase()
                                                        );


                                                    let dynamicLeagueName =
                                                        league
                                                            .abbreviation ||
                                                        league.name ||
                                                        "International";


                                                    /*
                                                     * Use the header for
                                                     * discovery, then query the
                                                     * normal series scoreboard
                                                     * for full competition
                                                     * objects.
                                                     */
                                                    if (
                                                        !fetchTasks
                                                            .some(
                                                                task =>
                                                                    task.url ===
                                                                    dynamicUrl
                                                            )
                                                    ) {
                                                        let currentParams =
                                                            "";


                                                        if (
                                                            isCricketInternationalPast
                                                        ) {
                                                            currentParams =
                                                                "limit=100";


                                                        } else if (
                                                            fetchTasks.length >
                                                            0
                                                        ) {
                                                            currentParams =
                                                                fetchTasks[0]
                                                                    .params ||
                                                                "";
                                                        }


                                                        fetchTasks.push({
                                                            url:
                                                                dynamicUrl,

                                                            league:
                                                                dynamicLeagueName,

                                                            params:
                                                                currentParams
                                                        });
                                                    }
                                                }
                                            );
                                    }


                                } catch (e) {
                                    console.log(
                                        "[ESPN HEADER PARSE ERROR] " +
                                        e
                                    );
                                }


                                console.log(
                                    "[ESPN DISCOVERY COMPLETE] " +
                                    sportString +
                                    " tasks=" +
                                    fetchTasks.length
                                );


                                finishHeaderDiscovery(
                                    false
                                );


                                return;
                            }


                            /*
                             * Retry one transient 5xx/429 response.
                             */
                            if (
                                (
                                    headerReq.status >=
                                        500 ||
                                    headerReq.status ==
                                        429
                                ) &&
                                headerRetryCount <
                                    1
                            ) {
                                headerRetryCount++;


                                console.log(
                                    "[ESPN HEADER RETRY] status=" +
                                    headerReq.status +
                                    " sport=" +
                                    sportString
                                );


                                setTimeout(
                                    fetchHeader,
                                    1000
                                );


                                return;
                            }


                            console.log(
                                "[ESPN DISCOVERY FALLBACK] status=" +
                                headerReq.status +
                                " sport=" +
                                sportString +
                                " tasks=" +
                                fetchTasks.length
                            );


                            finishHeaderDiscovery(
                                true
                            );
                        };


                    headerReq.onerror =
                        function() {

                            if (
                                isHeaderTimeout
                            ) {
                                return;
                            }


                            clearTimeout(
                                headerWatchdog
                            );


                            if (
                                headerRetryCount <
                                1
                            ) {
                                headerRetryCount++;


                                console.log(
                                    "[ESPN HEADER RETRY] network sport=" +
                                    sportString
                                );


                                setTimeout(
                                    fetchHeader,
                                    1000
                                );


                                return;
                            }


                            console.log(
                                "[ESPN DISCOVERY FALLBACK] network sport=" +
                                sportString +
                                " tasks=" +
                                fetchTasks.length
                            );


                            finishHeaderDiscovery(
                                true
                            );
                        };


                    headerReq.send();
                };


            fetchHeader();


        } else {
            if (
                fetchTasks.length ===
                0
            ) {
                checkCompletion();

            } else {
                executeFetchTasks();
            }
        }


    } else {
        if (
            fetchTasks.length ===
            0
        ) {
            checkCompletion();

        } else {
            executeFetchTasks();
        }
    }
}


// =========================================================
// FETCH INDIVIDUAL GAME
// =========================================================

function getGame(
    id,
    sport,
    onLoad,
    onError
) {
    getGamesForSport(
        sport,
        null,

        games => {

            let foundGame =
                games.find(
                    game =>
                        game.id == id ||
                        game.eventId == id
                );


            if (
                foundGame ==
                undefined
            ) {
                onError();

            } else {
                onLoad(
                    foundGame
                );
            }
        },

        onError
    );
}


// =========================================================
// PARSE ESPN GAME EVENT
// =========================================================

function parseEvent(
    sport,
    league,
    event,
    sourceUrl
) {
    const competition =
        event.competitions &&
        event.competitions.length > 0
            ? event.competitions[0]
            : null;


    if (!competition) {
        return null;
    }


    const competitors =
        competition.competitors ||
        [];


    const date =
        new Date(
            competition.date
        );


    const status =
        competition.status || {
            type: {
                name:
                    "STATUS_SCHEDULED",

                shortDetail:
                    ""
            }
        };


    const statusType =
        status.type || {
            name:
                "STATUS_SCHEDULED",

            shortDetail:
                ""
        };


    /*
     * UI STATE NORMALIZATION
     * ----------------------
     *
     * details = left footer field.
     * time    = center clock/status field.
     *
     * Live therefore becomes:
     *
     *     details = "LIVE"
     *     time    = ESPN shortDetail
     */
    let [
        details,
        time
    ] = (
        function(type) {

            const shortDetail =
                String(
                    statusType
                        .shortDetail ||
                    ""
                )
                    .trim();


            const validDate =
                !isNaN(
                    date.getTime()
                );


            // ---------------------------------------------
            // FINAL
            // ---------------------------------------------

            if (
                type ===
                "STATUS_FINAL"
            ) {
                return [
                    validDate
                        ? utils
                            .dateToScheduleDate(
                                date
                            )
                        : "Final",

                    "Final"
                ];
            }


            // ---------------------------------------------
            // POSTPONED / CANCELED
            // ---------------------------------------------

            if (
                type ===
                "STATUS_POSTPONED"
            ) {
                return [
                    "Postponed",
                    "TBA"
                ];
            }


            if (
                type ===
                    "STATUS_CANCELED" ||
                type ===
                    "STATUS_CANCELLED"
            ) {
                return [
                    "Canceled",
                    "TBA"
                ];
            }


            // ---------------------------------------------
            // CRICKET SCHEDULED
            // ---------------------------------------------

            if (
                type ===
                    "STATUS_SCHEDULED" &&
                sport ==
                    models.sports.CRICKET
            ) {
                let localDate =
                    validDate
                        ? utils
                            .dateToScheduleDate(
                                date
                            )
                        : "Scheduled";


                let localTime =
                    "";


                if (validDate) {
                    let hours =
                        date.getHours();

                    let minutes =
                        date.getMinutes();


                    let ampm =
                        hours >= 12
                            ? 'PM'
                            : 'AM';


                    hours =
                        hours % 12 ||
                        12;


                    localTime =
                        hours +
                        ':' +
                        (
                            minutes < 10
                                ? '0'
                                : ''
                        ) +
                        minutes +
                        ' ' +
                        ampm;


                } else {
                    localTime =
                        shortDetail ||
                        "Upcoming";
                }


                return [
                    localDate,
                    localTime
                ];
            }


            // ---------------------------------------------
            // OTHER SCHEDULED SPORTS
            // ---------------------------------------------

            if (
                type ===
                "STATUS_SCHEDULED"
            ) {
                return (
                    validDate
                        ? [
                            utils.dateToScheduleDate(
                                date
                            ),

                            utils.dateToScheduleTime(
                                date
                            )
                        ]
                        : [
                            "Scheduled",
                            "TBA"
                        ]
                );
            }


            // ---------------------------------------------
            // LIVE / IN PROGRESS
            // ---------------------------------------------

            return [
                "LIVE",

                shortDetail
                    ? shortDetail.replace(
                        "- ",
                        ""
                    )
                    : "LIVE"
            ];
        }
    )(
        statusType.name ||
        "STATUS_SCHEDULED"
    );


    /*
     * ESPN occasionally reports midnight as a placeholder.
     */
    if (
        time === "0:00 AM" ||
        time === "0:00am" ||
        time === "12:00 AM" ||
        time === "12:00am"
    ) {
        time =
            "TBA";
    }


    const id =
        event.id ||
        "0";


    const eventId =
        event.eventId ||
        id;


    /*
     * Remember the exact scoreboard route that produced this game.
     *
     * Keep useful query parameters such as dates= and limit=100 while
     * removing the stale cache-buster.
     */
    if (sourceUrl) {
        gameScoreboardUrls[id] =
            stripCacheBuster(
                sourceUrl
            );
    }


    const competitor1 =
        competitors.length > 1
            ? competitors[1]
            : {};


    const competitor2 =
        competitors.length > 0
            ? competitors[0]
            : {};


    const team1 =
        competitor1.team ||
        competitor1.athlete || {
            id:
                "0"
        };


    const team2 =
        competitor2.team ||
        competitor2.athlete || {
            id:
                "0"
        };


    /*
     * Pebble has very little horizontal score-screen room.
     */
    let t1Abbrev =
        String(
            team1.abbreviation ||
            team1.shortName ||
            team1.lastName ||
            "TBD"
        )
            .trim();


    let t2Abbrev =
        String(
            team2.abbreviation ||
            team2.shortName ||
            team2.lastName ||
            "TBD"
        )
            .trim();


    if (
        t1Abbrev.length >
        5
    ) {
        t1Abbrev =
            t1Abbrev
                .substring(
                    0,
                    5
                )
                .trim();
    }


    if (
        t2Abbrev.length >
        5
    ) {
        t2Abbrev =
            t2Abbrev
                .substring(
                    0,
                    5
                )
                .trim();
    }


    /*
     * Do not display fake 0-0 scores before an event begins.
     */
    const noScoreYet =
        statusType.name ===
            "STATUS_SCHEDULED" ||
        statusType.name ===
            "STATUS_POSTPONED" ||
        statusType.name ===
            "STATUS_CANCELED" ||
        statusType.name ===
            "STATUS_CANCELLED";


    let score1 =
        noScoreYet
            ? ""
            : String(
                competitor1.score ==
                    null
                    ? ""
                    : competitor1.score
            );


    let score2 =
        noScoreYet
            ? ""
            : String(
                competitor2.score ==
                    null
                    ? ""
                    : competitor2.score
            );


    // -----------------------------------------------------
    // TENNIS SET SCORING
    // -----------------------------------------------------

    if (
        sport ==
            models.sports.TENNIS &&
        !noScoreYet
    ) {
        /*
         * Some Tennis competitions omit the aggregate set score.
         */
        if (
            !score1 &&
            competitor1.linescores
        ) {
            let sets =
                0;


            competitor1
                .linescores
                .forEach(
                    line => {

                        if (
                            line.winner
                        ) {
                            sets++;
                        }
                    }
                );


            score1 =
                sets.toString();
        }


        if (
            !score2 &&
            competitor2.linescores
        ) {
            let sets =
                0;


            competitor2
                .linescores
                .forEach(
                    line => {

                        if (
                            line.winner
                        ) {
                            sets++;
                        }
                    }
                );


            score2 =
                sets.toString();
        }


        if (!score1) {
            score1 =
                "0";
        }


        if (!score2) {
            score2 =
                "0";
        }
    }


    // -----------------------------------------------------
    // CRICKET SCORE CLEANUP
    // -----------------------------------------------------

    if (
        sport ==
        models.sports.CRICKET
    ) {
        /*
         * Remove parenthetical overflow that does not fit on Pebble.
         */
        if (score1) {
            score1 =
                score1
                    .split(" (")[0]
                    .trim();
        }


        if (score2) {
            score2 =
                score2
                    .split(" (")[0]
                    .trim();
        }


        /*
         * For multi-innings strings, display the latest innings in the compact
         * score field.
         */
        if (
            score1 &&
            score1.indexOf("&") !==
                -1
        ) {
            score1 =
                score1
                    .split("&")
                    .pop()
                    .trim();
        }


        if (
            score2 &&
            score2.indexOf("&") !==
                -1
        ) {
            score2 =
                score2
                    .split("&")
                    .pop()
                    .trim();
        }
    }


    /*
     * Possession is meaningful only while a game is in progress.
     */
    const possession =
        statusType.name !=
            "STATUS_IN_PROGRESS"
            ? models.possession.NONE
            : gamePossession(
                sport,
                competition.situation,
                team1,
                team2
            );


    const team1Record =
        (
            competitor1.records &&
            competitor1.records.length >
                0
        )
            ? competitor1
                .records[0]
                .summary
            : (
                competitor1.record &&
                competitor1.record.length >
                    0
            )
                ? competitor1
                    .record[0]
                    .summary
                : "";


    const team2Record =
        (
            competitor2.records &&
            competitor2.records.length >
                0
        )
            ? competitor2
                .records[0]
                .summary
            : (
                competitor2.record &&
                competitor2.record.length >
                    0
            )
                ? competitor2
                    .record[0]
                    .summary
                : "";


    let broadcast =
        "Unlisted";


    if (
        competition.broadcasts &&
        competition.broadcasts.length >
            0
    ) {
        let names =
            competition
                .broadcasts[0]
                .names;


        if (
            names &&
            names.length > 0 &&
            names[0].trim() !==
                ""
        ) {
            broadcast =
                names[0];
        }
    }


    /*
     * Dynamic Cricket series use names such as "Sheff Shield" or tour names
     * and therefore do not have dedicated LEAGUE_OFFSETS entries.
     *
     * Keep all dynamic Cricket teams inside the International namespace so
     * favorites remain stable.
     */
    let offset =
        LEAGUE_OFFSETS[
            league
        ];


    if (
        offset === undefined &&
        sport ==
        models.sports.CRICKET
    ) {
        offset =
            LEAGUE_OFFSETS[
                "International"
            ];
    }


    if (
        offset === undefined
    ) {
        offset =
            0;
    }


    const rawT1Id =
        team1.id ||
        competitor1.id ||
        "0";


    const rawT2Id =
        team2.id ||
        competitor2.id ||
        "0";


    const parsedT1 =
        parseInt(
            rawT1Id,
            10
        );


    const parsedT2 =
        parseInt(
            rawT2Id,
            10
        );


    const t1Id =
        String(
            (
                isNaN(parsedT1)
                    ? 0
                    : parsedT1
            ) +
            offset
        );


    const t2Id =
        String(
            (
                isNaN(parsedT2)
                    ? 0
                    : parsedT2
            ) +
            offset
        );


    var gameObj =
        new models.Game(
            id,
            sport,

            new models.Team(
                t1Abbrev
                    .toUpperCase(),

                t1Id,
                team1Record,

                competitor1.winner ===
                    true
            ),

            score1,

            new models.Team(
                t2Abbrev
                    .toUpperCase(),

                t2Id,
                team2Record,

                competitor2.winner ===
                    true
            ),

            score2,
            possession,
            time,
            details,
            broadcast
        );


    /*
     * JS-only metadata.
     */
    gameObj.eventId =
        eventId;

    gameObj.startTime =
        date;

    gameObj.league =
        league;


    return gameObj;
}


// =========================================================
// LIVE GAME DETAILS
// =========================================================

function gameDetails(
    sport,
    situation
) {
    if (
        situation == undefined ||
        situation == null
    ) {
        return "";
    }


    switch (sport) {

        case models.sports.NFL:
            return (
                situation
                    .downDistanceText ||
                ""
            );


        case models.sports.MLB:

            if (
                situation.balls ===
                    undefined ||
                situation.strikes ===
                    undefined
            ) {
                return (
                    situation.outs !==
                        undefined
                        ? situation.outs +
                            " outs"
                        : ""
                );
            }


            return (
                situation.balls +
                "-" +
                situation.strikes +
                ", " +
                situation.outs +
                " outs"
            );


        default:
            return "";
    }
}


// =========================================================
// GAME POSSESSION
// =========================================================

function gamePossession(
    sport,
    situation,
    team1,
    team2
) {
    if (
        situation ==
            undefined ||
        situation ==
            null
    ) {
        return (
            models.possession.NONE
        );
    }


    switch (sport) {

        case models.sports.NFL:
            return possessionByTeam(
                situation.possession,
                team1,
                team2
            );


        case models.sports.MLB:

            if (
                situation.batter &&
                situation.batter.athlete &&
                situation.batter
                    .athlete
                    .team
            ) {
                return possessionByTeam(
                    situation
                        .batter
                        .athlete
                        .team
                        .id,

                    team1,
                    team2
                );


            } else if (
                situation.dueUp &&
                situation.dueUp.length >
                    0 &&
                situation.dueUp[0]
                    .athlete &&
                situation.dueUp[0]
                    .athlete
                    .team
            ) {
                return possessionByTeam(
                    situation
                        .dueUp[0]
                        .athlete
                        .team
                        .id,

                    team1,
                    team2
                );
            }


            return (
                models.possession.NONE
            );


        default:
            return (
                models.possession.NONE
            );
    }
}


function possessionByTeam(
    possessionId,
    team1,
    team2
) {
    if (
        possessionId ==
        team1.id
    ) {
        return (
            models.possession.TEAM1
        );
    }


    if (
        possessionId ==
        team2.id
    ) {
        return (
            models.possession.TEAM2
        );
    }


    return (
        models.possession.NONE
    );
}


// =========================================================
// TIMELINE ICONS
// =========================================================

function getTimelineIcon(
    sport
) {
    switch (sport) {

        case models.sports.NFL:
            return (
                "system://images/AMERICAN_FOOTBALL"
            );


        case models.sports.MLB:
            return (
                "system://images/TIMELINE_BASEBALL"
            );


        case models.sports.NHL:
            return (
                "system://images/HOCKEY_GAME"
            );


        case models.sports.NBA:
            return (
                "system://images/BASKETBALL"
            );


        case models.sports.MLS:
            return (
                "system://images/SOCCER_GAME"
            );


        case models.sports.CRICKET:
            return (
                "system://images/CRICKET_GAME"
            );


        case models.sports.TENNIS:
            return (
                "system://images/TIMELINE_SPORTS"
            );


        default:
            return (
                "system://images/TIMELINE_SPORTS"
            );
    }
}


// =========================================================
// LOCAL TIMELINE PIN INSERT
// =========================================================

function insertUserPin(
    pin
) {
    if (
        typeof Pebble !==
            'undefined' &&
        typeof Pebble
            .insertTimelinePin ===
            'function'
    ) {
        try {
            /*
             * RePebble's local insert API is fire-and-forget.
             */
            Pebble.insertTimelinePin(
                pin
            );


            console.log(
                "Local pin successfully pushed to OS: " +
                pin.id
            );


            return true;


        } catch (e) {
            console.log(
                "Error inserting local pin: " +
                e
            );


            return false;
        }


    } else {
        console.log(
            "Local insertTimelinePin not available on this platform."
        );


        return false;
    }
}


// =========================================================
// TIMELINE FAVORITES SYNC
// =========================================================

function updateTimelinePins(
    games
) {
    /*
     * Only one Timeline sync loop should exist at a time.
     */
    if (
        s_timeline_sync_timer
    ) {
        clearInterval(
            s_timeline_sync_timer
        );


        s_timeline_sync_timer =
            null;
    }


    const now =
        new Date();


    const future72h =
        new Date(
            now.getTime() +
            72 *
            60 *
            60 *
            1000
        );


    const past12h =
        new Date(
            now.getTime() -
            12 *
            60 *
            60 *
            1000
        );


    let pushedPins =
        {};


    try {
        let parsed =
            JSON.parse(
                localStorage
                    .getItem(
                        "pushed_pins_v4"
                    )
            );


        if (
            parsed &&
            typeof parsed ===
                'object' &&
            !Array.isArray(
                parsed
            )
        ) {
            pushedPins =
                parsed;
        }


    } catch (e) {}


    let pinsToPush =
        [];


    games.forEach(
        game => {

            if (
                game.startTime &&
                !isNaN(
                    game.startTime
                        .getTime()
                )
            ) {
                if (
                    game.startTime >
                        past12h &&
                    game.startTime <
                        future72h
                ) {
                    const pinId =
                        "game-" +
                        game.sport +
                        "-" +
                        game.id;


                    const localTimeISO =
                        game.startTime
                            .toISOString();


                    if (
                        pushedPins[
                            pinId
                        ] ===
                        localTimeISO
                    ) {
                        return;
                    }


                    let bodyText =
                        "Starts at: " +
                        game.time +
                        " (" +
                        game.details +
                        ")";


                    if (
                        game.broadcast
                    ) {
                        bodyText +=
                            "\nWatch on: " +
                            game.broadcast;
                    }


                    var pin = {
                        "id":
                            pinId,

                        "time":
                            localTimeISO,

                        "duration":
                            180,

                        "layout": {
                            "type":
                                "genericPin",

                            "title":
                                game.team1.name +
                                " vs " +
                                game.team2.name,

                            "subtitle":
                                "Get ready for the game!",

                            "body":
                                bodyText,

                            "tinyIcon":
                                getTimelineIcon(
                                    game.sport
                                ),

                            "largeIcon":
                                getTimelineIcon(
                                    game.sport
                                )
                        }
                    };


                    pinsToPush.push({
                        pin:
                            pin,

                        iso:
                            localTimeISO
                    });
                }
            }
        }
    );


    if (
        pinsToPush.length ===
        0
    ) {
        return;
    }


    let index =
        0;


    /*
     * Space local Timeline inserts one second apart.
     */
    s_timeline_sync_timer =
        setInterval(
            function() {

                if (
                    index >=
                    pinsToPush.length
                ) {
                    clearInterval(
                        s_timeline_sync_timer
                    );


                    s_timeline_sync_timer =
                        null;


                    /*
                     * Bound localStorage growth.
                     */
                    let keys =
                        Object.keys(
                            pushedPins
                        );


                    if (
                        keys.length >
                        100
                    ) {
                        let newPushedPins =
                            {};


                        keys
                            .slice(-100)
                            .forEach(
                                key => {
                                    newPushedPins[
                                        key
                                    ] =
                                        pushedPins[
                                            key
                                        ];
                                }
                            );


                        pushedPins =
                            newPushedPins;
                    }


                    try {
                        localStorage
                            .setItem(
                                "pushed_pins_v4",
                                JSON.stringify(
                                    pushedPins
                                )
                            );


                    } catch (e) {}


                    console.log(
                        "Timeline background sync complete."
                    );


                    return;
                }


                let item =
                    pinsToPush[
                        index
                    ];


                if (
                    insertUserPin(
                        item.pin
                    )
                ) {
                    pushedPins[
                        item.pin.id
                    ] =
                        item.iso;
                }


                index++;
            },

            1000
        );
}


// =========================================================
// WATCH BREAKDOWN REQUEST LISTENER
// =========================================================

Pebble.addEventListener(
    'appmessage',

    function(e) {

        let dict =
            e.payload;


        if (
            dict.REQUEST_BREAKDOWN
        ) {
            let sport =
                dict.REQUEST_BREAKDOWN;

            let gameId =
                dict.UPDATE_GAME_ID;

            let requestId =
                dict.REQUEST_ID;

            let leagueName =
                dict.SEND_GAME_LEAGUE ||
                "";


            getGameBreakdown(
                sport,
                leagueName,
                gameId,
                requestId,

                function(
                    responseDict
                ) {
                    Pebble.sendAppMessage(
                        responseDict,

                        function() {
                            console.log(
                                "[BREAKDOWN] Sent successfully to watch."
                            );
                        },

                        function(err) {
                            console.log(
                                "[BREAKDOWN] Failed to send to watch: " +
                                JSON.stringify(
                                    err
                                )
                            );
                        }
                    );
                }
            );
        }
    }
);


// =========================================================
// EXPORTS
// =========================================================

module.exports.getGames =
    getGames;

module.exports.getGame =
    getGame;