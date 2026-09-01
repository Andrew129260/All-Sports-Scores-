const fs = require('fs');
const api = require('./src/pkjs/api');
const models = require('./src/pkjs/models');

const fakeTennisData = {
    events: [
        {
            id: "event1",
            name: "Grand Slam",
            competitions: [
                {
                    id: "t1",
                    date: "2024-01-01T12:00Z",
                    competitors: [
                        { athlete: { displayName: "Player 1", shortName: "P. One" }, score: "2", winner: true },
                        { athlete: { displayName: "Player 2", shortName: "P. Two" }, score: "0", winner: false }
                    ],
                    status: {
                        type: { name: "STATUS_FINAL" }
                    }
                }
            ]
        }
    ]
};

const gameObj = api.getGame("t1", models.sports.TENNIS, (game) => {
    console.log(game.team1.winner, game.team2.winner);
}, () => {});
