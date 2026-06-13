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
    console.log("sportGroups = ", JSON.stringify(sportGroups));
    const favoriteSports = Object.keys(sportGroups).map(key => parseInt(key));
    console.log("favoriteSports = ", JSON.stringify(favoriteSports));
    var favoriteGames = [];
    var loadedSports = [];
    Object.values(sportGroups).forEach((sportGroup) => {
        const sport = sportGroup[0].sport
        const teamIDs = sportGroup.map(favoriteItem => favoriteItem.teamID)
        getGamesForSport(
            sport,
            (games) => {
                console.log("loaded sport = ", sport, ", games = ", JSON.stringify(games));
                console.log("teamIDs = ", JSON.stringify(teamIDs), "includes 23 = ", teamIDs.includes("23"));
                const filtered = games.filter(game => teamIDs.includes(game.team1.id) || teamIDs.includes(game.team2.id) );
                console.log("filtered = ", JSON.stringify(filtered));
                favoriteGames.push(...filtered);
                loadedSports.push(sport);
                console.log("favoriteGames = ", JSON.stringify(favoriteGames), ", loadedSports = ", JSON.stringify(loadedSports), ", favoriteSports = ", JSON.stringify(favoriteSports));
                if (favoriteSports.every(sport => loadedSports.includes(sport))) {
                    console.log("loaded all sports");
                    onLoad(favoriteGames);
                }
            },
            onError 
        )
    })
}

function getEndpointForSport(sport) {
    var endpoint = "https://site.api.espn.com/apis/site/v2/sports";
    switch (sport) {
        case models.sports.NFL: endpoint += '/football/nfl'; break;
        case models.sports.MLB: endpoint += '/baseball/mlb'; break;
        case models.sports.NHL: endpoint += '/hockey/nhl'; break;
        case models.sports.NBA: endpoint += '/basketball/nba'; break;
        case models.sports.MLS: endpoint += '/soccer/usa.1'; break;
        case models.sports.RUGBY: endpoint += '/rugby/rugby-league'; break;
        case models.sports.CRICKET: endpoint += '/cricket/8039'; break;
        default: break;
    }
    return endpoint;
}

function getGamesForSport(sport, onLoad, onError) {
    if (DEBUG_MOCK) return mock.nfl;
    var req = new XMLHttpRequest();
    const endpoint = getEndpointForSport(sport) + "/scoreboard"
    console.log("getting games from endpoint = ", endpoint);
    req.open('GET', endpoint);
    req.onload = function (e) {
        if (req.readyState == 4 && req.status == 200) {
            const sportsData = JSON.parse(req.responseText);
            console.log("parsed sportsData")
            const events = sportsData.events;
            const games = events.map(event => parseEvent(sport, event));
            onLoad(games);
            return;
        }
        onError();
    }
    req.onerror = function (e) {
        onError()
    }
    req.send();
}

function getGame(id, sport, onLoad, onError) {
    console.log("getting game ", id, " for sport = ", sport);
    getGamesForSport(
        sport, 
        (games) => { 
            let foundGame = games.find(g => g.id == id)
            console.log("foundGame = ", JSON.stringify(foundGame));
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

    // Truncate massive Cricket score payloads to fit on the watchface
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

    return new models.Game(
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

module.exports.getGames = getGames;
module.exports.getGame = getGame;