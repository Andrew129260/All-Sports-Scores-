const fs = require('fs');

const fakeTennisData = {
    events: [
        {
            id: "t1",
            name: "Grand Slam",
            groupings: [{
                competitions: [
                    {
                        id: "t1",
                        date: new Date().toISOString(),
                        competitors: [
                            { athlete: { displayName: "Player 1", shortName: "P. One" }, score: "2", winner: true },
                            { athlete: { displayName: "Player 2", shortName: "P. Two" }, score: "0", winner: false }
                        ],
                        status: {
                            type: { name: "STATUS_FINAL" }
                        }
                    }
                ]
            }]
        }
    ]
};

global.XMLHttpRequest = class {
    open() {}
    send() {
        this.readyState = 4;
        this.status = 200;
        this.responseText = JSON.stringify(fakeTennisData);
        if (this.onload) this.onload();
    }
}
const api = require('./src/pkjs/api');
const models = require('./src/pkjs/models');

api.getGames(models.sports.TENNIS, 0, (games) => {
    console.log("Found games:", games.length);
    console.log(games[0].team1.name, games[0].team1.winner, games[0].team2.name, games[0].team2.winner);
}, () => {});
