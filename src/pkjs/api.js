const models = require('./models');
const storage = require('./storage');
const utils = require('./utils');
const mock = require('./mock');

const DEBUG_MOCK = false;

/*
 * Keep these limits synchronized with games-handler.c.
 */
const MAX_GAMES_DEFAULT = 50;
const MAX_GAMES_APLITE = 5;
const MAX_GAMES_HIGH_MEMORY = 150;

function getGames(sport, leagueIndex, onLoad, onError) {
    if (sport == models.sports.FAVORITES) {
        getFavoriteGames(storage.storedFavorites(), onLoad, onError);
    } else {
        getGamesForSport(sport, leagueIndex, onLoad, onError);
    }
}

function getFavoriteGames(favorites, onLoad, onError) {
    if (favorites.length == 0) {
        onLoad([]);
        return;
    }

    const sportGroups = utils.groupBy(
        favorites,
        favoriteItem => favoriteItem.sport
    );

    const favoriteSports =
        Object.keys(sportGroups).map(
            key => parseInt(key)
        );

    const favoriteGames = [];
    const loadedSports = [];
    let hasError = false;

    Object.values(sportGroups).forEach((sportGroup) => {
        const sport = sportGroup[0].sport;
        const teamIDs =
            sportGroup.map(
                favoriteItem => favoriteItem.teamID
            );

        getGamesForSport(
            sport,
            null,
            (games) => {

                const filtered =
                    games.filter(
                        game =>
                            teamIDs.includes(game.team1.id) ||
                            teamIDs.includes(game.team2.id)
                    );

                favoriteGames.push(...filtered);
                loadedSports.push(sport);

                if (
                    favoriteSports.every(
                        s => loadedSports.includes(s)
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
                loadedSports.push(sport);

                if (
                    favoriteSports.every(
                        s => loadedSports.includes(s)
                    )
                ) {
                    if (favoriteGames.length > 0) {
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
            }
        );
    });
}

function getEndpointsForSport(sport) {
    /*
     * Use site.api.espn.com.
     */
    var base =
        "https://site.api.espn.com/apis/site/v2/sports";

    switch (sport) {

        case models.sports.NFL:
            return [
                {
                    url: base + '/football/nfl',
                    league: "NFL"
                },
                {
                    url: base + '/football/college-football',
                    league: "NCAAF",
                    params: "&groups=80"
                },
                {
                    url: base + '/football/ufl',
                    league: "UFL"
                },
                {
                    url: base + '/football/cfl',
                    league: "CFL"
                }
            ];

        case models.sports.MLB:
            return [
                {
                    url: base + '/baseball/mlb',
                    league: "MLB"
                },
                {
                    url: base + '/baseball/college-baseball',
                    league: "NCAA Base"
                },
                {
                    url: base + '/baseball/world-baseball-classic',
                    league: "WBC"
                }
            ];

        case models.sports.NHL:
            return [
                {
                    url: base + '/hockey/nhl',
                    league: "NHL"
                },
                {
                    url: base + '/hockey/mens-college-hockey',
                    league: "NCAA Hockey"
                }
            ];

        case models.sports.NBA:
            return [
                {
                    url: base + '/basketball/nba',
                    league: "NBA"
                },
                {
                    url: base + '/basketball/wnba',
                    league: "WNBA"
                },
                {
                    url: base + '/basketball/mens-college-basketball',
                    league: "NCAAM",
                    params: "&groups=50"
                },
                {
                    url: base + '/basketball/fiba',
                    league: "FIBA Men"
                },
                {
                    url: base + '/basketball/fiba',
                    league: "FIBA Women"
                }
            ];

        case models.sports.MLS:
            return [
                {
                    url: base + '/soccer/usa.1',
                    league: "MLS"
                },
                {
                    url: base + '/soccer/eng.1',
                    league: "EPL"
                },
                {
                    url: base + '/soccer/esp.1',
                    league: "La Liga"
                },
                {
                    url: base + '/soccer/ger.1',
                    league: "Bundesliga"
                },
                {
                    url: base + '/soccer/ita.1',
                    league: "Serie A"
                },
                {
                    url: base + '/soccer/mex.1',
                    league: "Liga MX"
                },
                {
                    url: base + '/soccer/uefa.champions',
                    league: "UEFA Champ"
                },
                {
                    url: base + '/soccer/fifa.world',
                    league: "World Cup"
                }
            ];

        case models.sports.RUGBY:
            return [
                {
                    url: base + '/rugby-league/3',
                    league: "NRL"
                },
                {
                    url: base + '/rugby/180659',
                    league: "Six Nations"
                },
                {
                    url: base + '/rugby/164205',
                    league: "Rugby WC"
                }
            ];

        case models.sports.CRICKET:
            return [
                {
                    url: [
                        base + '/cricket/8039',
                        base + '/cricket/8040'
                    ],
                    league: "International"
                },
                {
                    url: base + '/cricket/8048',
                    league: "IPL"
                },
                {
                    url: base + '/cricket/1528556',
                    league: "MLC"
                }
            ];

        case models.sports.TENNIS:
            return [
                {
                    url: base + '/tennis/atp',
                    league: "ATP"
                },
                {
                    url: base + '/tennis/wta',
                    league: "WTA"
                }
            ];

        case models.sports.AFL:
            return [
                {
                    url: base + '/australian-football/afl',
                    league: "AFL"
                }
            ];

        case models.sports.MMA:
            return [
                {
                    url: base + '/mma/ufc',
                    league: "UFC"
                }
            ];

        default:
            return [];
    }
}

function getGamesForSport(
    sport,
    leagueIndex,
    onLoad,
    onError
) {
    if (DEBUG_MOCK) {
        onLoad(mock.nfl);
        return;
    }

    let endpoints =
        getEndpointsForSport(sport);

    let allGames = [];
    let completedRequests = 0;
    let hasCriticalError = false;
    let hasLoaded = false;

    if (endpoints.length === 0) {
        onError();
        return;
    }

    if (
        leagueIndex !== undefined &&
        leagueIndex !== null &&
        leagueIndex >= 0 &&
        leagueIndex < endpoints.length
    ) {
        endpoints = [
            endpoints[leagueIndex]
        ];
    }

    let fetchTasks = [];

    endpoints.forEach(endpoint => {

        let urls =
            Array.isArray(endpoint.url)
                ? endpoint.url
                : [endpoint.url];

        urls.forEach(u => {
            fetchTasks.push({
                url: u,
                league: endpoint.league,
                params: endpoint.params || ""
            });
        });
    });

    /*
     * IMPORTANT TEST:
     *
     * Do NOT send dates= or limit= to ESPN.
     *
     * The bare ESPN scoreboard endpoint is known to return
     * data, while our previous rolling date-range requests
     * were returning HTTP 400.
     *
     * Keep endpoint-specific parameters such as groups=80
     * and groups=50.
     */
    const queryParams = "";

    fetchTasks.forEach(task => {
        task.params += queryParams;
    });

    function finishSuccess(games) {
        if (hasLoaded) return;

        hasLoaded = true;
        onLoad(games);
    }

    function finishError() {
        if (hasLoaded) return;

        hasLoaded = true;
        onError();
    }

    function executeFetchTasks() {
        let activeRequests = 0;
        let taskIndex = 0;

        const MAX_CONCURRENT = 4;

        function runNext() {

            if (taskIndex >= fetchTasks.length) {
                return;
            }

            let task =
                fetchTasks[taskIndex++];

            activeRequests++;

            let req =
                new XMLHttpRequest();

            /*
             * The only automatic query parameter is our cache
             * buster. ESPN receives no dates= or limit=.
             */
            const fullUrl =
                task.url +
                "/scoreboard?t=" +
                Date.now() +
                task.params;

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

                        console.log(
                            "XHR Watchdog Timeout for: " +
                            fullUrl
                        );

                        hasCriticalError =
                            true;

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
                        req.readyState != 4
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
                        req.status == 200
                    ) {

                        try {

                            let sportsData =
                                JSON.parse(
                                    req.responseText
                                );


                            if (
                                sportsData.events
                            ) {

                                let allParsedEvents =
                                    [];


                                const processCompetition =
                                    (
                                        comp,
                                        event
                                    ) => {

                                    let status =
                                        comp.status &&
                                        comp.status.type
                                            ? comp.status.type.name
                                            : "";


                                    let shortDetail =
                                        comp.status &&
                                        comp.status.type
                                            ? (
                                                comp.status.type.shortDetail ||
                                                ""
                                              )
                                            : "";


                                    let p1 =
                                        comp.competitors &&
                                        comp.competitors.length > 1
                                            ? (
                                                comp.competitors[1].athlete ||
                                                comp.competitors[1].team
                                              )
                                            : null;


                                    let p2 =
                                        comp.competitors &&
                                        comp.competitors.length > 0
                                            ? (
                                                comp.competitors[0].athlete ||
                                                comp.competitors[0].team
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


                                    if (
                                        name1 === "TBD" &&
                                        name2 === "TBD"
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


                                sportsData.events.forEach(
                                    event => {

                                    if (
                                        event.competitions
                                    ) {

                                        event.competitions.forEach(
                                            comp =>
                                                processCompetition(
                                                    comp,
                                                    event
                                                )
                                        );

                                    } else if (
                                        event.groupings
                                    ) {

                                        event.groupings.forEach(
                                            grouping => {

                                            if (
                                                grouping.competitions
                                            ) {

                                                grouping.competitions.forEach(
                                                    comp =>
                                                        processCompetition(
                                                            comp,
                                                            event
                                                        )
                                                );
                                            }
                                        });
                                    }
                                });


                                let games =
                                    allParsedEvents
                                        .map(
                                            event =>
                                                parseEvent(
                                                    sport,
                                                    task.league,
                                                    event
                                                )
                                        )
                                        .filter(
                                            g => g !== null
                                        );


                                allGames =
                                    allGames.concat(
                                        games
                                    );


                                allParsedEvents =
                                    null;

                                sportsData =
                                    null;

                                games =
                                    null;
                            }


                        } catch (e) {

                            console.log(
                                "[ESPN JSON ERROR] " +
                                fullUrl +
                                " :: " +
                                e
                            );
                        }

                    } else if (
                        req.status != 404
                    ) {

                        hasCriticalError =
                            true;

                        console.log(
                            "[ESPN HTTP ERROR] " +
                            req.status +
                            " :: " +
                            fullUrl
                        );
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

                    console.log(
                        "[ESPN NETWORK ERROR] " +
                        fullUrl
                    );

                    completedRequests++;
                    activeRequests--;

                    checkCompletion();
                    runNext();
                };


            req.send();
        }


        for (
            let i = 0;
            i < MAX_CONCURRENT &&
            i < fetchTasks.length;
            i++
        ) {
            runNext();
        }
    }


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
                });


            uniqueGames.sort(
                (a, b) => {

                    const getWeight =
                        (game) => {

                        let isFinal =
                            game.time === "Final" ||
                            game.time === "FT" ||
                            (
                                game.time &&
                                game.time
                                    .toLowerCase()
                                    .indexOf(
                                        "final"
                                    ) > -1
                            );


                        let isScheduled =
                            game.time &&
                            (
                                game.time
                                    .toLowerCase()
                                    .indexOf(
                                        "am"
                                    ) > -1 ||

                                game.time
                                    .toLowerCase()
                                    .indexOf(
                                        "pm"
                                    ) > -1 ||

                                game.time
                                    .toLowerCase()
                                    .indexOf(
                                        "tbd"
                                    ) > -1 ||

                                game.time
                                    .toLowerCase() ===
                                        "scheduled"
                            );


                        if (
                            !isFinal &&
                            !isScheduled &&
                            game.time
                        ) {
                            return 0;
                        }

                        return 1;
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
                        return (
                            a.startTime.getTime() -
                            b.startTime.getTime()
                        );
                    }


                    return 0;
                }
            );


            /*
             * Local filtering remains exactly what we want.
             *
             * ESPN decides what its default scoreboard contains;
             * we make sure old/far-future games don't reach the watch.
             */
            const nowTime =
                new Date();


            const futureLimit =
                new Date(
                    nowTime.getTime() +
                    (
                        14 *
                        24 *
                        60 *
                        60 *
                        1000
                    )
                );


            const pastLimit =
                new Date(
                    nowTime.getTime() -
                    (
                        14 *
                        24 *
                        60 *
                        60 *
                        1000
                    )
                );


            let filteredGames =
                uniqueGames.filter(
                    game => {

                    if (
                        game.startTime &&
                        !isNaN(
                            game.startTime.getTime()
                        )
                    ) {

                        return (
                            game.startTime >=
                                pastLimit &&
                            game.startTime <=
                                futureLimit
                        );
                    }

                    return true;
                });


            let maxGames =
                MAX_GAMES_DEFAULT;


            if (
                typeof Pebble !== 'undefined' &&
                typeof Pebble.getActiveWatchInfo ===
                    'function'
            ) {

                let watchInfo =
                    Pebble.getActiveWatchInfo();


                if (watchInfo) {

                    const platform =
                        String(
                            watchInfo.platform ||
                            ""
                        ).toLowerCase();


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

            console.log(
                "[ESPN FAILURE] " +
                "No games and at least one request failed."
            );


            finishError();


        } else {

            console.log(
                "[ESPN EMPTY] " +
                "No games returned."
            );


            finishSuccess(
                []
            );
        }
    }


    /*
     * Keep your existing dynamic discovery mechanism for now.
     *
     * IMPORTANT:
     * Its header request also has NO dates= parameter.
     */
    if (
        leagueIndex === undefined ||
        leagueIndex === null ||
        leagueIndex === 0
    ) {

        let sportString = "";

        switch (sport) {

            case models.sports.NFL:
                sportString =
                    "football";
                break;

            case models.sports.MLB:
                sportString =
                    "baseball";
                break;

            case models.sports.NHL:
                sportString =
                    "hockey";
                break;

            case models.sports.NBA:
                sportString =
                    "basketball";
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

            case models.sports.AFL:
                sportString =
                    "australian-football";
                break;

            case models.sports.MMA:
                sportString =
                    "mma";
                break;

            case models.sports.TENNIS:
                sportString =
                    "tennis";
                break;
        }


        if (
            sportString !== ""
        ) {

            let headerReq =
                new XMLHttpRequest();


            /*
             * No dates parameter here either.
             */
            let headerUrl =
                'https://site.api.espn.com/apis/personalized/v2/scoreboard/header?sport=' +
                encodeURIComponent(
                    sportString
                ) +
                '&t=' +
                Date.now();


            console.log(
                "[ESPN HEADER] " +
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

                        console.log(
                            "Dynamic Discovery Watchdog Timeout"
                        );


                        try {
                            headerReq.abort();
                        } catch (e) {}


                        if (
                            fetchTasks.length ===
                            0
                        ) {
                            finishError();
                        } else {
                            executeFetchTasks();
                        }

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
                        headerReq.status
                    );


                    if (
                        headerReq.status ==
                        200
                    ) {

                        try {

                            let headerData =
                                JSON.parse(
                                    headerReq.responseText
                                );


                            if (
                                headerData.sports &&
                                headerData.sports.length >
                                    0
                            ) {

                                let activeLeagues =
                                    headerData
                                        .sports[0]
                                        .leagues;


                                if (
                                    activeLeagues
                                ) {

                                    activeLeagues.forEach(
                                        league => {

                                        if (
                                            !league.id
                                        ) {
                                            return;
                                        }

                                        // FIX: Use the named slug if available to prevent 400 Errors
                                        let leagueIdentifier = league.slug || league.abbreviation || league.id;

                                        let dynamicUrl =
                                            "https://site.api.espn.com/apis/site/v2/sports/" +
                                            encodeURIComponent(
                                                sportString
                                            ) +
                                            "/" +
                                            encodeURIComponent(
                                                String(leagueIdentifier).toLowerCase()
                                            );


                                        if (
                                            !fetchTasks.some(
                                                t =>
                                                    t.url ===
                                                    dynamicUrl
                                            )
                                        ) {

                                            console.log(
                                                "[DYNAMIC DISCOVERY] Added Active " +
                                                sportString.toUpperCase() +
                                                " Tour ID: " +
                                                league.id +
                                                " (" +
                                                (
                                                    league.name ||
                                                    "Tour"
                                                ) +
                                                ")"
                                            );


                                            const currentParams =
                                                fetchTasks.length >
                                                0
                                                    ? fetchTasks[0].params
                                                    : "";


                                            fetchTasks.push({
                                                url:
                                                    dynamicUrl,

                                                league:
                                                    league.abbreviation ||
                                                    "International",

                                                params:
                                                    currentParams
                                            });
                                        }
                                    });
                                }
                            }

                        } catch (e) {

                            console.log(
                                "Dynamic Header Parse Error"
                            );
                        }
                    }


                    if (
                        fetchTasks.length ===
                        0
                    ) {
                        finishError();
                    } else {
                        executeFetchTasks();
                    }
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


                    console.log(
                        "[ESPN HEADER NETWORK ERROR]"
                    );


                    if (
                        fetchTasks.length ===
                        0
                    ) {
                        finishError();
                    } else {
                        executeFetchTasks();
                    }
                };


            headerReq.send();

        } else {

            if (
                fetchTasks.length ===
                0
            ) {
                finishError();
            } else {
                executeFetchTasks();
            }
        }

    } else {

        if (
            fetchTasks.length ===
            0
        ) {
            finishError();
        } else {
            executeFetchTasks();
        }
    }
}

function getGame(
    id,
    sport,
    onLoad,
    onError
) {
    getGamesForSport(
        sport,
        null,
        (games) => {

            let foundGame =
                games.find(
                    g =>
                        g.id == id ||
                        g.eventId == id
                );

            if (
                foundGame == undefined
            ) {
                onError();
            } else {
                onLoad(foundGame);
            }
        },
        onError
    );
}

function parseEvent(
    sport,
    league,
    event
) {
    const competition =
        event.competitions &&
        event.competitions.length > 0
            ? event.competitions[0]
            : null;

    if (!competition) return null;

    const competitors =
        competition.competitors || [];

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


    const [details, time] =
        (function(type) {

        if (
            sport == 7 ||
            league === "MLC" ||
            league === 2
        ) {

            let localTime = "";
            let localDate = "";


            if (
                date &&
                !isNaN(
                    date.getTime()
                )
            ) {

                localDate =
                    utils.dateToScheduleDate(
                        date
                    );


                let hours =
                    date.getHours();

                let minutes =
                    date.getMinutes();


                let ampm =
                    hours >= 12
                        ? 'PM'
                        : 'AM';


                hours =
                    hours % 12;

                hours =
                    hours
                        ? hours
                        : 12;


                minutes =
                    minutes < 10
                        ? '0' + minutes
                        : minutes;


                localTime =
                    hours +
                    ':' +
                    minutes +
                    ' ' +
                    ampm;

            } else {

                localDate =
                    "Scheduled";


                localTime =
                    (
                        status.type &&
                        status.type.shortDetail
                    )
                        ? status.type.shortDetail
                        : "Upcoming";
            }


            return [
                localDate,
                localTime
            ];
        }


        switch (type) {

            case "STATUS_FINAL":
                return [
                    utils.dateToScheduleDate(
                        date
                    ),
                    "Final"
                ];


            case "STATUS_SCHEDULED":
                return [
                    utils.dateToScheduleDate(
                        date
                    ),
                    utils.dateToScheduleTime(
                        date
                    )
                ];


            default:
                return [
                    gameDetails(
                        sport,
                        competition.situation
                    ),
                    (
                        status.type &&
                        status.type.shortDetail
                    )
                        ? status.type.shortDetail.replace(
                            "- ",
                            ""
                          )
                        : ""
                ];
        }

    })(
        status.type
            ? status.type.name
            : "STATUS_SCHEDULED"
    );


    const id =
        event.id ||
        "0";

    const eventId =
        event.eventId ||
        id;


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
        competitor1.athlete ||
        {
            id: "0"
        };


    const team2 =
        competitor2.team ||
        competitor2.athlete ||
        {
            id: "0"
        };


    let t1Abbrev =
        String(
            team1.abbreviation ||
            team1.shortName ||
            team1.lastName ||
            "TBD"
        ).trim();


    let t2Abbrev =
        String(
            team2.abbreviation ||
            team2.shortName ||
            team2.lastName ||
            "TBD"
        ).trim();


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


    let score1 =
        status.type.name ==
            "STATUS_SCHEDULED"
                ? ""
                : String(
                    competitor1.score ||
                    ""
                );


    let score2 =
        status.type.name ==
            "STATUS_SCHEDULED"
                ? ""
                : String(
                    competitor2.score ||
                    ""
                );


    if (
        sport ==
        models.sports.TENNIS
    ) {

        if (
            !score1 &&
            competitor1.linescores
        ) {

            let sets = 0;

            competitor1.linescores.forEach(
                l => {
                    if (l.winner) {
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

            let sets = 0;

            competitor2.linescores.forEach(
                l => {
                    if (l.winner) {
                        sets++;
                    }
                }
            );

            score2 =
                sets.toString();
        }


        if (
            !score1 &&
            status.type.name !=
                "STATUS_SCHEDULED"
        ) {
            score1 = "0";
        }


        if (
            !score2 &&
            status.type.name !=
                "STATUS_SCHEDULED"
        ) {
            score2 = "0";
        }
    }


    if (
        sport ==
        models.sports.CRICKET
    ) {

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


        if (
            score1 &&
            score1.indexOf("&") !== -1
        ) {
            score1 =
                score1
                    .split("&")
                    .pop()
                    .trim();
        }


        if (
            score2 &&
            score2.indexOf("&") !== -1
        ) {
            score2 =
                score2
                    .split("&")
                    .pop()
                    .trim();
        }
    }


    const possession =
        status.type.name !=
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
            competitor1.records.length > 0
        )
            ? competitor1.records[0].summary
            : (
                competitor1.record &&
                competitor1.record.length > 0
            )
                ? competitor1.record[0].summary
                : "";


    const team2Record =
        (
            competitor2.records &&
            competitor2.records.length > 0
        )
            ? competitor2.records[0].summary
            : (
                competitor2.record &&
                competitor2.record.length > 0
            )
                ? competitor2.record[0].summary
                : "";


    let broadcast = "";


    if (
        competition.broadcasts &&
        competition.broadcasts.length > 0
    ) {

        let names =
            competition.broadcasts[0].names;


        if (
            names &&
            names.length > 0
        ) {
            broadcast =
                names[0];
        }
    }


    const t1Id =
        team1.id ||
        competitor1.id ||
        "0";


    const t2Id =
        team2.id ||
        competitor2.id ||
        "0";


    var gameObj =
        new models.Game(

            id,
            sport,

            new models.Team(
                t1Abbrev.toUpperCase(),
                t1Id,
                team1Record,
                competitor1.winner === true
            ),

            score1,

            new models.Team(
                t2Abbrev.toUpperCase(),
                t2Id,
                team2Record,
                competitor2.winner === true
            ),

            score2,

            possession,
            time,
            details,
            broadcast
        );


    gameObj.eventId =
        eventId;

    gameObj.startTime =
        date;

    gameObj.league =
        league;


    return gameObj;
}

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
                situation.downDistanceText ||
                ""
            );

        case models.sports.MLB:

            if (
                situation.balls === undefined ||
                situation.strikes === undefined
            ) {
                return (
                    situation.outs !== undefined
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

function gamePossession(
    sport,
    situation,
    team1,
    team2
) {
    if (
        situation == undefined ||
        situation == null
    ) {
        return models.possession.NONE;
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
                situation.batter.athlete.team
            ) {

                return possessionByTeam(
                    situation.batter.athlete.team.id,
                    team1,
                    team2
                );

            } else if (
                situation.dueUp &&
                situation.dueUp.length > 0 &&
                situation.dueUp[0].athlete &&
                situation.dueUp[0].athlete.team
            ) {

                return possessionByTeam(
                    situation.dueUp[0].athlete.team.id,
                    team1,
                    team2
                );
            }

            return models.possession.NONE;

        default:
            return models.possession.NONE;
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
        return models.possession.TEAM1;
    }

    if (
        possessionId ==
        team2.id
    ) {
        return models.possession.TEAM2;
    }

    return models.possession.NONE;
}

function getTimelineIcon(
    sport
) {
    switch (sport) {

        case models.sports.NFL:
            return "system://images/AMERICAN_FOOTBALL";

        case models.sports.MLB:
            return "system://images/TIMELINE_BASEBALL";

        case models.sports.NHL:
            return "system://images/HOCKEY_GAME";

        case models.sports.NBA:
            return "system://images/BASKETBALL";

        case models.sports.MLS:
            return "system://images/SOCCER_GAME";

        case models.sports.CRICKET:
            return "system://images/CRICKET_GAME";

        case models.sports.TENNIS:
            return "system://images/TIMELINE_SPORTS";

        default:
            return "system://images/TIMELINE_SPORTS";
    }
}

function insertUserPin(
    pin
) {
    if (
        typeof Pebble !== 'undefined' &&
        typeof Pebble.insertTimelinePin ===
            'function'
    ) {

        Pebble.insertTimelinePin(
            pin
        );

        console.log(
            "Local pin insertion requested: " +
            pin.id
        );

    } else {

        var sendRequest =
            function(token) {

            var req =
                new XMLHttpRequest();

            req.open(
                'PUT',
                'https://timeline-api.getpebble.com/v1/user/pins/' +
                encodeURIComponent(
                    pin.id
                ),
                true
            );

            req.setRequestHeader(
                'Content-Type',
                'application/json'
            );

            req.setRequestHeader(
                'X-User-Token',
                '' + token
            );

            req.onload =
                function() {

                console.log(
                    "Timeline API Response: " +
                    req.status +
                    " " +
                    req.responseText
                );
            };

            req.send(
                JSON.stringify(pin)
            );
        };


        if (
            typeof Pebble !== 'undefined' &&
            typeof Pebble.getTimelineToken ===
                'function'
        ) {

            Pebble.getTimelineToken(

                function(token) {
                    sendRequest(
                        token
                    );
                },

                function(error) {

                    console.log(
                        'Failed to get timeline token (' +
                        error +
                        '), attempting offline local pin push anyway'
                    );

                    sendRequest(
                        'offline-dummy-token'
                    );
                }
            );
        }
    }
}

function updateTimelinePins(
    games
) {
    const now =
        new Date();

    const future72h =
        new Date(
            now.getTime() +
            (
                72 *
                60 *
                60 *
                1000
            )
        );

    let pushedPins = {};

    try {

        let parsed =
            JSON.parse(
                localStorage.getItem(
                    "pushed_pins"
                )
            );

        if (
            parsed &&
            typeof parsed ===
                'object' &&
            !Array.isArray(parsed)
        ) {

            pushedPins =
                parsed;

        } else if (
            Array.isArray(parsed)
        ) {

            parsed.forEach(
                id => {
                    pushedPins[id] =
                        true;
                }
            );
        }

    } catch (e) {}


    games.forEach(
        game => {

        if (
            game.startTime &&
            !isNaN(
                game.startTime.getTime()
            )
        ) {

            if (
                game.startTime > now &&
                game.startTime < future72h
            ) {

                const pinId =
                    "game-" +
                    game.id;

                const localTimeISO =
                    game.startTime
                        .toISOString();


                if (
                    pushedPins[pinId] ===
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


                if (game.broadcast) {
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


                insertUserPin(
                    pin
                );

                pushedPins[pinId] =
                    localTimeISO;
            }
        }
    });


    let keys =
        Object.keys(
            pushedPins
        );


    if (
        keys.length > 100
    ) {

        let newPushedPins = {};

        keys.slice(-100).forEach(
            k => {
                newPushedPins[k] =
                    pushedPins[k];
            }
        );

        pushedPins =
            newPushedPins;
    }


    localStorage.setItem(
        "pushed_pins",
        JSON.stringify(
            pushedPins
        )
    );
}

module.exports.getGames =
    getGames;

module.exports.getGame =
    getGame;