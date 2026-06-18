const sports = {
    FAVORITES: 0,
    NFL: 1,
    MLB: 2,
    NHL: 3,
    NBA: 4,
    MLS: 5,
    RUGBY: 6,
    CRICKET: 7,
    TENNIS: 8,
    AFL: 9,
    MMA: 10
};

const possession = { TEAM1: 0, TEAM2: 1, NONE: 2 };
const gameslistdata = { LIST_ITEM: 0, LAST_LIST_ITEM: 1, NO_GAMES: 2, NETWORK_ERROR: 3, INIT_ARRAY: 4 };
const updategamedata = { UPDATE_GAME: 0, NETWORK_ERROR: 1 };

function Team(abbreviation, id, record) {
    this.abbreviation = abbreviation;
    this.name = abbreviation;
    this.id = id;
    this.record = record;
    this.score = "";
}

function Game(id, sport, team1, score1, team2, score2, possession, time, details, broadcast) {
    this.id = id;
    this.sport = sport;
    this.team1 = team1;
    this.score1 = score1;
    this.team2 = team2;
    this.score2 = score2;
    this.possession = possession;
    this.time = time;
    this.details = details;
    this.broadcast = broadcast;
    this.startTime = null;
    this.league = "";
}

function FavoriteTeam(sport, teamID) {
    this.sport = sport;
    this.teamID = teamID;
}

module.exports = { sports, possession, gameslistdata, updategamedata, Team, Game, FavoriteTeam };
