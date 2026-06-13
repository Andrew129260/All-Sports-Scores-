const models = require('./models');
const storage = require('./storage');
const utils = require('./utils');
const mock = require('./mock');

const DEBUG_MOCK = false;

function getGames(sport, onLoad, onError) {
    if (sport == models.sports.FAVORITES) { 
        getFavoriteGames(storage.storedFavorites(), onLoad, onError);
    } else {
        getGamesForSport(sport, onLoad, onError)
    }
}

function getFavoriteGames(favorites, onLoad, onError) {
    if(favorites.length == 0) {
        onLoad([]);
        return;
    }
    console.log("getting favorite games");
    const sportGroups = utils.groupBy(favorites, favoriteItem => favoriteItem.sport);
    const favoriteSports = Object.keys(sportGroups).map(key => parseInt(key));
    var favoriteGames = [];
    var loadedSports = [];
    
    Object.values(sportGroups).forEach((sportGroup) => {
        const sport = sportGroup[0].sport
        const teamIDs = sportGroup.map(favoriteItem => favoriteItem.teamID)
        getGamesForSport(
            sport,
            (games) => {
                const filtered = games.filter(game => teamIDs.includes(game.team1.id) || teamIDs.includes(game.team2.id) );
                favoriteGames.push(...filtered);
                loadedSports.push(sport);
                
                if (favoriteSports.every(sport => loadedSports.includes(sport))) {
                    console.log("loaded all sports");
                    
                    // Trigger the Timeline Pin logic for all future favorite games!
                    updateTimelinePins(favoriteGames);
                    
                    onLoad(favoriteGames);
                }
            },
            onError 
        )
    })
}

function getEndpointsForSport(sport) {
    var base = "https://site.api.espn.com/apis/site/v2/sports";
    switch (sport) {
        case models.sports.NFL: return [
            base + '/football/nfl', 
            base + '/football/college-football' // NCAAF
        ];
        case models.sports.MLB: return [
            base + '/baseball/mlb', 
            base + '/baseball/college-baseball' // NCAA Baseball
        ];
        case models.sports.NHL: return [
            base + '/hockey/nhl',
            base + '/hockey/mens-college-hockey' // NCAA Hockey
        ];
        case models.sports.NBA: return [
            base + '/basketball/nba', 
            base + '/basketball/wnba', 
            base + '/basketball/mens-college-basketball' // NCAAM
        ];
        case models.sports.MLS: return [
            base + '/soccer/usa.1', // MLS
            base + '/soccer/eng.1', // English Premier League
            base + '/soccer/esp.1', // La Liga
            base + '/soccer/uefa.champions' // Champions League
        ];
        case models.sports.RUGBY: return [
            base + '/rugby-league/3', // NRL
            base + '/rugby/180659' // Six Nations
        ];
        case models.sports.CRICKET: return [
            base + '/cricket/8039', // International
            base + '/cricket/8048' // Indian Premier League (IPL)
        ];
        default: return [];
    }
}

function getGamesForSport(sport, onLoad, onError) {
    if (DEBUG_MOCK) return mock.nfl;
    
    const endpoints = getEndpointsForSport(sport);
    let allGames = [];
    let completedRequests = 0;
    let hasCriticalError = false;

    if (endpoints.length === 0) {
        onError();
        return;
    }

    // Fire a request for every URL in the array simultaneously
    endpoints.forEach(endpoint => {
        var req = new XMLHttpRequest();
        const fullUrl = endpoint + "/scoreboard";
        console.log("getting games from endpoint = ", fullUrl);
        
        req.open('GET', fullUrl);
        req.onload = function () {
            if (req.readyState == 4) {
                if (req.status == 200) {
                    try {
                        const sportsData = JSON.parse(req.responseText);
                        if (sportsData.events) {
                            const games = sportsData.events.map(event => parseEvent(sport, event));
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
            console.log("Network error for: " + fullUrl);
            hasCriticalError = true;
            completedRequests++;
            checkCompletion();
        };
        req.send();
    });

    // Only update the watch UI once ALL requests have finished checking in
    function checkCompletion() {
        if (completedRequests === endpoints.length) {
            if (allGames.length > 0) {
                
                // DEDUPLICATION: Ensure no game IDs are repeated before sending to the watch
                const uniqueGames = [];
                const seenIds = new Set();
                allGames.forEach(game => {
                    if (!seenIds.has(game.id)) {
                        seenIds.add(game.id);
                        uniqueGames.push(game);
                    }
                });

                onLoad(uniqueGames);
                
            } else if (hasCriticalError) {
                onError(); 
            } else {
                onLoad([]); 
            }
        }
    }
}

function getGame(id, sport, onLoad, onError) {
    getGamesForSport(
        sport, 
        (games) => { 
            let foundGame = games.find(g => g.id == id)
            if(foundGame == undefined) {
                onError();
            } else {
                onLoad(foundGame);
            }
         },
        onError)
}

function parseEvent(sport, event) {
    const competitors = event.competitions[0].competitors;
    const date = new Date(Date.parse(event.competitions[0].date));
    const status = event.competitions[0].status;
    const [details, time] = (function(type) {
        switch (type) {
            case "STATUS_FINAL": return [utils.dateToScheduleDate(date), "Final"];
            case "STATUS_SCHEDULED": return [utils.dateToScheduleDate(date), utils.dateToScheduleTime(date)];
            default: return [gameDetails(sport, event.competitions[0].situation), status.type.shortDetail.replace("- ", "")];
        }
    })(status.type.name);

    const id = event.id;

    const competitor1 = competitors[1]; 
    const competitor2 = competitors[0];
    const team1 = competitor1.team;
    const team2 = competitor2.team;

    let score1 = status.type.name == "STATUS_SCHEDULED" ? "" : competitor1.score;
    let score2 = status.type.name == "STATUS_SCHEDULED" ? "" : competitor2.score;

    if (sport == models.sports.CRICKET) {
        if (score1) score1 = score1.toString().split(" (")[0];
        if (score2) score2 = score2.toString().split(" (")[0];
    }

    const possession = status.type.name != "STATUS_IN_PROGRESS" ? models.possession.NONE : gamePossession(sport, event.competitions[0].situation, team1, team2);

    const team1Record = (competitor1.records && competitor1.records.length > 0) ? competitor1.records[0].summary : 
                      (competitor1.record && competitor1.record.length > 0) ? competitor1.record[0].summary : "0-0";
                      
    const team2Record = (competitor2.records && competitor2.records.length > 0) ? competitor2.records[0].summary : 
                      (competitor2.record && competitor2.record.length > 0) ? competitor2.record[0].summary : "0-0";

    let broadcast = "";
    if (event.competitions[0].broadcasts && event.competitions[0].broadcasts.length > 0) {
        let names = event.competitions[0].broadcasts[0].names;
        if (names && names.length > 0) {
            broadcast = names[0];
        }
    }

    var gameObj = new models.Game(
        id,
        sport,
        new models.Team(team1.abbreviation, team1.id, team1Record),
        score1,
        new models.Team(team2.abbreviation, team2.id, team2Record),
        score2,
        possession,
        time,
        details,
        broadcast 
    );

    // Save the raw Date object to the game so we can do our 48-hour Timeline math
    gameObj.startTime = date; 
    
    return gameObj;
}

function gameDetails(sport, situation) {
    if(situation == undefined || situation == null) return "";
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
    switch (sport) {
        case models.sports.NFL: return possessionByTeam(situation.possession, team1, team2);
        case models.sports.MLB: return situation.batter == undefined ? possessionByTeam(situation.dueUp[0].athlete.team.id, team1, team2) : possessionByTeam(situation.batter.athlete.team.id, team1, team2);
        default: return models.possession.NONE;
    }
}

function possessionByTeam(possessionID, team1, team2) {
    return (team1.id == possessionID) ? models.possession.TEAM1 : (team2.id == possessionID) ? models.possession.TEAM2 : models.possession.NONE;
}

// ==========================================
// TIMELINE PIN LOGIC
// ==========================================

function getTimelineIcon(sport) {
    // Map our sports to Pebble's built-in timeline system icons
    switch (sport) {
        case models.sports.NFL: return "system://images/AMERICAN_FOOTBALL";
        case models.sports.MLB: return "system://images/BASEBALL";
        case models.sports.NHL: return "system://images/HOCKEY_GAME";
        case models.sports.NBA: return "system://images/BASKETBALL";
        case models.sports.MLS: return "system://images/SOCCER_GAME";
        case models.sports.CRICKET: return "system://images/CRICKET_GAME";
        default: return "system://images/GENERIC_SPORTS";
    }
}

function insertUserPin(pin) {
    console.log("Attempting to insert pin: " + JSON.stringify(pin));
    
    Pebble.getTimelineToken(function(token) {
        console.log("Success! Got timeline token: " + token);
        
        var req = new XMLHttpRequest();
        req.open('PUT', 'https://timeline-api.rebble.io/v1/user/pins/' + pin.id, true);
        req.setRequestHeader('Content-Type', 'application/json');
        req.setRequestHeader('X-User-Token', '' + token);
        
        req.onload = function() {
            console.log('Core Devices Intercept Status: ' + req.status);
        };
        req.onerror = function() {
            console.log('Error pushing timeline pin (Network/Intercept error)');
        }
        
        req.send(JSON.stringify(pin));
        console.log("Pin payload sent to interceptor!");
        
    }, function(error) {
        console.log('CRITICAL ERROR: Failed to get timeline token: ' + error);
    });
}

function updateTimelinePins(games) {
    const now = new Date();
    const future48h = new Date(now.getTime() + (48 * 60 * 60 * 1000));
    
    console.log("Timeline Check: Now = " + now.toISOString() + " | Limit = " + future48h.toISOString());

    games.forEach(game => {
        if (game.startTime) {
            console.log("Checking Game ID " + game.id + " | Start = " + game.startTime.toISOString());
            
            if (game.startTime > now && game.startTime < future48h) {
                console.log("Game falls in 48h window! Building pin...");
                
                let bodyText = "Starts at: " + game.time + " (" + game.details + ")";
                if (game.broadcast) {
                    bodyText += "\nWatch on: " + game.broadcast;
                }

                // Core Devices timezone fix: Shift the UTC time to match your local time digits
                let localOffset = game.startTime.getTimezoneOffset() * 60000;
                let localTimeISO = new Date(game.startTime.getTime() - localOffset).toISOString();

                var pin = {
                    "id": "game-" + game.id,
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
            } else {
                console.log("Game rejected: outside 48 hour window.");
            }
        } else {
            console.log("Game rejected: Invalid start time.");
        }
    });
}

module.exports.getGames = getGames;
module.exports.getGame = getGame;
