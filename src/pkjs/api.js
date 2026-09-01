const models = require('./models');
const storage = require('./storage');
const utils = require('./utils');
const mock = require('./mock');

const DEBUG_MOCK = false;

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
    
    const sportGroups = utils.groupBy(favorites, favoriteItem => favoriteItem.sport);
    const favoriteSports = Object.keys(sportGroups).map(key => parseInt(key));
    const favoriteGames = [];
    const loadedSports = [];
    let hasError = false;

    Object.values(sportGroups).forEach((sportGroup) => {
        const sport = sportGroup[0].sport;
        const teamIDs = sportGroup.map(favoriteItem => favoriteItem.teamID);
        
        getGamesForSport(
            sport,
            null, 
            (games) => {
                const filtered = games.filter(game => teamIDs.includes(game.team1.id) || teamIDs.includes(game.team2.id));
                favoriteGames.push(...filtered);
                loadedSports.push(sport);
                
                if (favoriteSports.every(s => loadedSports.includes(s))) {
                    if (favoriteGames.length > 0 || !hasError) {
                        updateTimelinePins(favoriteGames);
                        onLoad(favoriteGames);
                    } else {
                        onError();
                    }
                }
            },
            () => {
                hasError = true;
                // FIX: Acknowledge completion even if it failed so the watch doesn't hang!
                loadedSports.push(sport); 
                
                if (favoriteSports.every(s => loadedSports.includes(s))) {
                    if (favoriteGames.length > 0) {
                        updateTimelinePins(favoriteGames);
                        onLoad(favoriteGames);
                    } else {
                        onError();
                    }
                }
            } 
        );
    });
}

function getEndpointsForSport(sport) {
    var base = "https://site.api.espn.com/apis/site/v2/sports";
    switch (sport) {
        case models.sports.NFL: return [
            { url: base + '/football/nfl', league: "NFL" }, 
            { url: base + '/football/college-football', league: "NCAAF", params: "&groups=80" }, 
            { url: base + '/football/ufl', league: "UFL" },
            { url: base + '/football/cfl', league: "CFL" }
        ];
        case models.sports.MLB: return [
            { url: base + '/baseball/mlb', league: "MLB" }, 
            { url: base + '/baseball/college-baseball', league: "NCAA Base" },
            { url: base + '/baseball/world-baseball-classic', league: "WBC" }
        ];
        case models.sports.NHL: return [
            { url: base + '/hockey/nhl', league: "NHL" },
            { url: base + '/hockey/mens-college-hockey', league: "NCAA Hockey" }
        ];
        case models.sports.NBA: return [
            { url: base + '/basketball/nba', league: "NBA" }, 
            { url: base + '/basketball/wnba', league: "WNBA" }, 
            { url: base + '/basketball/mens-college-basketball', league: "NCAAM", params: "&groups=50" }, 
            { url: base + '/basketball/fiba.mens.world.cup', league: "FIBA Men" },
            { url: base + '/basketball/fiba.womens.world.cup', league: "FIBA Women" }
        ];
        case models.sports.MLS: return [
            { url: base + '/soccer/usa.1', league: "MLS" }, 
            { url: base + '/soccer/eng.1', league: "EPL" }, 
            { url: base + '/soccer/esp.1', league: "La Liga" }, 
            { url: base + '/soccer/ger.1', league: "Bundesliga" },
            { url: base + '/soccer/ita.1', league: "Serie A" },
            { url: base + '/soccer/mex.1', league: "Liga MX" },
            { url: base + '/soccer/uefa.champions', league: "UEFA Champ" },
            { url: base + '/soccer/fifa.world', league: "World Cup" },
            { url: base + '/soccer/fifa.womens.world.cup', league: "Women's WC" }
        ];
        case models.sports.RUGBY: return [
            { url: base + '/rugby-league/3', league: "NRL" }, 
            { url: base + '/rugby/180659', league: "Six Nations" },
            { url: base + '/rugby/world-cup', league: "Rugby WC" }
        ];
        case models.sports.CRICKET: return [
            { url: [base + '/cricket/8039', base + '/cricket/8040'], league: "International" },
            { url: base + '/cricket/8048', league: "IPL" },
            { url: base + '/cricket/1528556', league: "MLC" } 
        ];
        case models.sports.TENNIS: return [
            { url: base + '/tennis/atp', league: "ATP" }, 
            { url: base + '/tennis/wta', league: "WTA" }
        ];
        case models.sports.AFL: return [
            { url: base + '/australian-football/afl', league: "AFL" }
        ];
        case models.sports.MMA: return [
            { url: base + '/mma/ufc', league: "UFC" }
        ];
        default: return [];
    }
}

function getGamesForSport(sport, leagueIndex, onLoad, onError) {
    if (DEBUG_MOCK) return mock.nfl;
    
    let endpoints = getEndpointsForSport(sport);
    let allGames = [];
    let completedRequests = 0;
    let hasCriticalError = false;

    if (endpoints.length === 0) {
        onError();
        return;
    }

    if (leagueIndex !== undefined && leagueIndex !== null && leagueIndex >= 0 && leagueIndex < endpoints.length) {
        endpoints = [endpoints[leagueIndex]];
    }

    let fetchTasks = [];
    endpoints.forEach(endpoint => {
        let urls = Array.isArray(endpoint.url) ? endpoint.url : [endpoint.url];
        urls.forEach(u => {
            fetchTasks.push({ url: u, league: endpoint.league, params: endpoint.params || "" });
        });
    });

    if (fetchTasks.length === 0) {
        onError();
        return;
    }

    function executeFetchTasks() {
        fetchTasks.forEach(task => {
            let req = new XMLHttpRequest();
            const fullUrl = task.url + "/scoreboard?t=" + Date.now() + task.params;
            
            req.open('GET', fullUrl);
            req.onload = function () {
                if (req.readyState == 4) {
                    if (req.status == 200) {
                        try {
                            const sportsData = JSON.parse(req.responseText);
                            if (sportsData.events) {
                                let allParsedEvents = [];
                                sportsData.events.forEach(event => {
                                    if (event.competitions) {
                                        allParsedEvents.push(event);
                                    } else if (event.groupings) {
                                        event.groupings.forEach(grouping => {
                                            if (grouping.competitions) {
                                                grouping.competitions.forEach(comp => {
                                                    // Skip cancelled/retired/walkover matches, and TBD vs TBD matches
                                                    // which bloat the tennis timeline and cause scrolling issues
                                                    let status = comp.status && comp.status.type ? comp.status.type.name : "";
                                                    let shortDetail = comp.status && comp.status.type ? (comp.status.type.shortDetail || "") : "";
                                                    let p1 = comp.competitors && comp.competitors.length > 1 ? (comp.competitors[1].athlete || comp.competitors[1].team) : null;
                                                    let p2 = comp.competitors && comp.competitors.length > 0 ? (comp.competitors[0].athlete || comp.competitors[0].team) : null;
                                                    let name1 = p1 ? (p1.displayName || p1.shortName || "TBD") : "TBD";
                                                    let name2 = p2 ? (p2.displayName || p2.shortName || "TBD") : "TBD";

                                                    if (status === "STATUS_RETIRED" || status === "STATUS_WALKOVER") {
                                                        return;
                                                    }
                                                    if (shortDetail.indexOf("Retired") !== -1 || shortDetail.indexOf("Walkover") !== -1) {
                                                        return;
                                                    }
                                                    if (name1 === "TBD" && name2 === "TBD") {
                                                        return;
                                                    }

                                                    allParsedEvents.push({
                                                        id: comp.id || event.id,
                                                        name: event.name,
                                                        competitions: [comp]
                                                    });
                                                });
                                            }
                                        });
                                    }
                                });
                                let games = allParsedEvents.map(event => parseEvent(sport, task.league, event)).filter(g => g !== null);
                                allGames = allGames.concat(games);
                            }
                        } catch (e) {
                            console.log("JSON Parse Error for: " + fullUrl);
                        }
                    } else if (req.status != 404) {
                        hasCriticalError = true;
                    }
                    
                    completedRequests++;
                    checkCompletion();
                }
            };
            req.onerror = function () {
                hasCriticalError = true;
                completedRequests++;
                checkCompletion();
            };
            req.send();
        });
    }

    function checkCompletion() {
        if (completedRequests === fetchTasks.length) {
            if (allGames.length > 0) {
                const seenIds = new Set();
                const uniqueGames = allGames.filter(game => {
                    if (seenIds.has(game.id)) return false;
                    seenIds.add(game.id);
                    return true;
                });

                uniqueGames.sort((a, b) => {
                    let aIsFinal = (a.time && a.time.toLowerCase().indexOf('final') !== -1);
                    let bIsFinal = (b.time && b.time.toLowerCase().indexOf('final') !== -1);

                    if (aIsFinal && !bIsFinal) return 1;
                    if (!aIsFinal && bIsFinal) return -1;

                    if (a.startTime && b.startTime) {
                        return a.startTime.getTime() - b.startTime.getTime();
                    }
                    return 0;
                });

                const now = new Date();
                const futureLimit = new Date(now.getTime() + (14 * 24 * 60 * 60 * 1000));
                const pastLimit = new Date(now.getTime() - (14 * 24 * 60 * 60 * 1000));
                const filteredGames = uniqueGames.filter(game => {
                    if (game.startTime && !isNaN(game.startTime.getTime())) {
                        return game.startTime >= pastLimit && game.startTime <= futureLimit;
                    }
                    return true;
                });

                onLoad(filteredGames);
            } else if (hasCriticalError) {
                onError(); 
            } else {
                onLoad([]); 
            }
        }
    }

    // Dynamic Discovery: Pre-Flight Check for BOTH Cricket and Rugby
    if ((sport == models.sports.CRICKET || sport == models.sports.RUGBY) && (leagueIndex === undefined || leagueIndex === null || leagueIndex === 0)) {
        let sportString = (sport == models.sports.CRICKET) ? "cricket" : "rugby";
        let headerReq = new XMLHttpRequest();
        headerReq.open('GET', 'https://site.web.api.espn.com/apis/personalized/v2/scoreboard/header?sport=' + sportString + '&t=' + Date.now());
        headerReq.onload = function () {
            if (headerReq.readyState == 4) {
                if (headerReq.status == 200) {
                    try {
                        let headerData = JSON.parse(headerReq.responseText);
                        if (headerData.sports && headerData.sports.length > 0) {
                            let activeLeagues = headerData.sports[0].leagues;
                            if (activeLeagues) {
                                activeLeagues.forEach(league => {
                                    if (league.id) {
                                        let dynamicUrl = "https://site.api.espn.com/apis/site/v2/sports/" + sportString + "/" + league.id;
                                        if (!fetchTasks.some(t => t.url === dynamicUrl)) {
                                            console.log("[DYNAMIC DISCOVERY] Added Active " + sportString.toUpperCase() + " Tour ID: " + league.id + " (" + (league.name || "Tour") + ")");
                                            fetchTasks.push({ url: dynamicUrl, league: league.abbreviation || "International", params: "" });
                                        }
                                    }
                                });
                            }
                        }
                    } catch (e) {
                        console.log("Dynamic Header Parse Error");
                    }
                }
                executeFetchTasks();
            }
        };
        headerReq.onerror = function () {
            executeFetchTasks();
        };
        headerReq.send();
    } else {
        executeFetchTasks();
    }
}

function getGame(id, sport, onLoad, onError) {
    getGamesForSport(sport, null, (games) => { 
            let foundGame = games.find(g => g.id == id);
            if (foundGame == undefined) {
                onError();
            } else {
                onLoad(foundGame);
            }
        },
        onError);
}

function parseEvent(sport, league, event) {
    const competition = event.competitions && event.competitions.length > 0 ? event.competitions[0] : null;
    if (!competition) return null;

    const competitors = competition.competitors || [];
    const date = new Date(Date.parse(competition.date));
    const status = competition.status || { type: { name: "STATUS_SCHEDULED", shortDetail: "" } };

    const [details, time] = (function(type) {
        if (sport == 7 || league === "MLC" || league == 2) {
            let localTime = "";
            let localDate = "";
            
            if (date && !isNaN(date.getTime())) {
                localDate = utils.dateToScheduleDate(date);
                
                let hours = date.getHours();
                let minutes = date.getMinutes();
                let ampm = hours >= 12 ? 'PM' : 'AM';
                hours = hours % 12;
                hours = hours ? hours : 12; 
                minutes = minutes < 10 ? '0' + minutes : minutes;
                
                localTime = hours + ':' + minutes + ' ' + ampm;
            } else {
                localDate = "Scheduled";
                localTime = (status.type && status.type.shortDetail) ? status.type.shortDetail : "Upcoming";
            }
            return [localDate, localTime];
        }

        switch (type) {
            case "STATUS_FINAL": 
                return [utils.dateToScheduleDate(date), "Final"];
            case "STATUS_SCHEDULED": 
                return [utils.dateToScheduleDate(date), utils.dateToScheduleTime(date)];
            default: 
                return [gameDetails(sport, competition.situation), ((status.type && status.type.shortDetail) ? status.type.shortDetail : "").replace("- ", "")];
        }
    })(status.type ? status.type.name : "STATUS_SCHEDULED");

    const id = event.id || "0";

    const competitor1 = competitors.length > 1 ? competitors[1] : {}; 
    const competitor2 = competitors.length > 0 ? competitors[0] : {};

    const team1 = competitor1.team || competitor1.athlete || { id: "0" };
    const team2 = competitor2.team || competitor2.athlete || { id: "0" };

    let t1Abbrev = String(team1.abbreviation || team1.shortName || team1.lastName || "TBD").trim();
    let t2Abbrev = String(team2.abbreviation || team2.shortName || team2.lastName || "TBD").trim();

    if (t1Abbrev.length > 5) t1Abbrev = t1Abbrev.substring(0, 5).trim();
    if (t2Abbrev.length > 5) t2Abbrev = t2Abbrev.substring(0, 5).trim();

    let score1 = status.type.name == "STATUS_SCHEDULED" ? "" : String(competitor1.score || "");
    let score2 = status.type.name == "STATUS_SCHEDULED" ? "" : String(competitor2.score || "");

    if (sport == models.sports.CRICKET) {
        // Trim overs and keep only the latest innings for compact displays
        if (score1) score1 = score1.split(" (")[0].trim();
        if (score2) score2 = score2.split(" (")[0].trim();

        if (score1 && score1.indexOf("&") !== -1) {
            score1 = score1.split("&").pop().trim();
        }
        if (score2 && score2.indexOf("&") !== -1) {
            score2 = score2.split("&").pop().trim();
        }
    }

    const possession = status.type.name != "STATUS_IN_PROGRESS" ? models.possession.NONE : gamePossession(sport, competition.situation, team1, team2);

    const team1Record = (competitor1.records && competitor1.records.length > 0) ? competitor1.records[0].summary : 
                      (competitor1.record && competitor1.record.length > 0) ? competitor1.record[0].summary : "";
                      
    const team2Record = (competitor2.records && competitor2.records.length > 0) ? competitor2.records[0].summary : 
                      (competitor2.record && competitor2.record.length > 0) ? competitor2.record[0].summary : "";

    let broadcast = "";
    if (competition.broadcasts && competition.broadcasts.length > 0) {
        let names = competition.broadcasts[0].names;
        if (names && names.length > 0) {
            broadcast = names[0];
        }
    }

    const t1Id = team1.id || competitor1.id || "0";
    const t2Id = team2.id || competitor2.id || "0";

    var gameObj = new models.Game(
        id,
        sport,
        new models.Team(t1Abbrev.toUpperCase(), t1Id, team1Record),
        score1,
        new models.Team(t2Abbrev.toUpperCase(), t2Id, team2Record),
        score2,
        possession,
        time,
        details,
        broadcast 
    );

    gameObj.startTime = date; 
    gameObj.league = league; 
    
    return gameObj;
}

function gameDetails(sport, situation) {
    if (situation == undefined || situation == null) return ""; 
    switch (sport) {
        case models.sports.NFL: 
            return situation.downDistanceText || "";
        case models.sports.MLB: 
            if (situation.balls === undefined || situation.strikes === undefined) {
                return situation.outs !== undefined ? situation.outs + " outs" : "";
            }
            return situation.balls + "-" + situation.strikes + ", " + situation.outs + " outs"; 
        default: 
            return "";
    }
}

function gamePossession(sport, situation, team1, team2) {
    if (situation == undefined || situation == null) return models.possession.NONE; 
    switch (sport) {
        case models.sports.NFL: 
            return possessionByTeam(situation.possession, team1, team2);
        case models.sports.MLB: 
            if (situation.batter && situation.batter.athlete && situation.batter.athlete.team) {
                return possessionByTeam(situation.batter.athlete.team.id, team1, team2);
            } else if (situation.dueUp && situation.dueUp.length > 0 && situation.dueUp[0].athlete && situation.dueUp[0].athlete.team) {
                return possessionByTeam(situation.dueUp[0].athlete.team.id, team1, team2);
            }
            return models.possession.NONE;
        default: 
            return models.possession.NONE;
    }
}

function possessionByTeam(possessionId, team1, team2) {
    if (possessionId == team1.id) return models.possession.TEAM1;
    if (possessionId == team2.id) return models.possession.TEAM2;
    return models.possession.NONE;
}

function getTimelineIcon(sport) {
    switch (sport) {
        case models.sports.NFL: return "system://images/AMERICAN_FOOTBALL";
        case models.sports.MLB: return "system://images/BASEBALL";
        case models.sports.NHL: return "system://images/HOCKEY_GAME";
        case models.sports.NBA: return "system://images/BASKETBALL";
        case models.sports.MLS: return "system://images/SOCCER_GAME";
        case models.sports.CRICKET: return "system://images/CRICKET_GAME";
        case models.sports.TENNIS: return "system://images/TENNIS_BALL";
        default: return "system://images/GENERIC_SPORTS"; 
    }
}

function insertUserPin(pin) {
    // Local Pins are supported in newer Pebble apps and work offline
    if (typeof Pebble.insertTimelinePin === 'function') {
        Pebble.insertTimelinePin(pin, function() {
            console.log("Local pin inserted successfully: " + pin.id);
        }, function(error) {
            console.log("Error inserting local pin: " + error);
        });
    } else {
        // Fallback: If we can get a timeline token, send it normally.
        // If getting the token fails (e.g. offline), STILL send the request
        // with a dummy token. The Pebble app on the phone intercepts
        // requests to timeline-api.rebble.io and creates local pins!
        var sendRequest = function(token) {
            var req = new XMLHttpRequest();
            req.open('PUT', 'https://timeline-api.rebble.io/v1/user/pins/' + pin.id, true);
            req.setRequestHeader('Content-Type', 'application/json');
            req.setRequestHeader('X-User-Token', '' + token);

            req.onload = function () {
                console.log("Timeline API Response: " + req.status + " " + req.responseText);
            };

            req.send(JSON.stringify(pin));
        };

        Pebble.getTimelineToken(function(token) {
            sendRequest(token);
        }, function(error) {
            console.log('Failed to get timeline token (' + error + '), attempting offline local pin push anyway');
            sendRequest('offline-dummy-token');
        });
    }
}

function updateTimelinePins(games) {
    const now = new Date();
    const future48h = new Date(now.getTime() + (48 * 60 * 60 * 1000));

    // CACHE FIX: Check localStorage to prevent spamming Rebble servers with duplicate pins
    let pushedPins = [];
    try { 
        let parsed = JSON.parse(localStorage.getItem("pushed_pins"));
        if (Array.isArray(parsed)) {
            pushedPins = parsed;
        }
    } catch(e) {}

    games.forEach(game => {
        if (game.startTime && !isNaN(game.startTime.getTime())) {
            if (game.startTime > now && game.startTime < future48h) {
                let pinId = "game-" + game.id;
                
                // SKIPPING: We already sent this to Rebble!
                if (pushedPins.includes(pinId)) return;

                let bodyText = "Starts at: " + game.time + " (" + game.details + ")";
                if (game.broadcast) { bodyText += "\nWatch on: " + game.broadcast; }

                let localTimeISO = game.startTime.toISOString();

                var pin = {
                    "id": pinId,
                    "time": localTimeISO,
                    "layout": {
                        "type": "genericPin",
                        "title": game.team1.name + " vs " + game.team2.name,
                        "subtitle": "Get ready for the game!",
                        "body": bodyText,
                        "tinyIcon": getTimelineIcon(game.sport),
                        "largeIcon": getTimelineIcon(game.sport)
                    }
                };
                
                insertUserPin(pin);
                pushedPins.push(pinId);
            }
        }
    });

    // Keep the cache size healthy so it doesn't blow up the phone's local storage limits
    if (pushedPins.length > 100) {
        pushedPins = pushedPins.slice(-100);
    }
    localStorage.setItem("pushed_pins", JSON.stringify(pushedPins));
}

module.exports.getGames = getGames;
module.exports.getGame = getGame;