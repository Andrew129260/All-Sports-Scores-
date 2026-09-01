var models = require('./models');
var storage = require('./storage');
var comms = require('./comms');
var api = require('./api');

var Clay = require('@rebble/clay');
var clayConfig = require('./config.json');
var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

Pebble.addEventListener('showConfiguration', function(e) {
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e && !e.response) {
    return;
  }

  // Get the keys and values from each config item
  var dict = clay.getSettings(e.response);
  console.log("Settings changed: " + JSON.stringify(dict));

  // Handle favorite teams
  var newFavorites = [];

  // Extract leagues favorite settings
  const leagues = ['NFL', 'MLB', 'NHL', 'NBA', 'MLS'];

  for (let l of leagues) {
      const key = 'FAVORITES_' + l;
      if (dict[key]) {
          let favs = dict[key];
          // dict[key] might be an array or string
          if (!Array.isArray(favs)) {
              favs = [favs];
          }
          for (let val of favs) {
              if (!val) continue;
              const parts = val.toString().split(':');
              if (parts.length === 2) {
                  newFavorites.push(new models.FavoriteTeam(parseInt(parts[0]), parts[1]));
              }
          }
      }
      // Remove these from dict before sending to watch (AppMessage) since watch doesn't need them
      delete dict[key];
  }

  // Update storage
  localStorage.setItem('favorites', JSON.stringify(newFavorites));
  storage.storedFavorites(true); // Maybe add a force reload?

  // Push settings to Pebble watch (leagues and display options)
  Pebble.sendAppMessage(dict, function(e) {
    console.log('Sent config data to Pebble');
  }, function(e) {
    console.log('Failed to send config data!');
    console.log(JSON.stringify(e));
  });

  // Trigger timeline push for new favorites
  api.getGames(models.sports.FAVORITES, null,
      function(games) {
          console.log("Timeline background push triggered by settings update.");
      },
      function(error) {
          console.log("Timeline background push failed.");
      }
  );
});



Pebble.addEventListener("ready", function(e) {
        Pebble.sendAppMessage({'READY': 1});
    }
);

// Get AppMessage events
Pebble.addEventListener('appmessage', function(e) {
    // Get the dictionary from the message
    var dict = e.payload;
  
    console.log('Got message: ' + JSON.stringify(dict));

    // every appmessage from this watch app should come with an associated request id
    if (!("REQUEST_ID" in dict)) { console.error("No request id!"); return;}
    const requestID = dict["REQUEST_ID"];

    switch(true) {
        case ("LOAD_GAMES" in dict):
            const sport = dict["LOAD_GAMES"];
            
            // Check if the watch requested a specific folder, otherwise default to null
            const leagueIndex = ("LEAGUE_INDEX" in dict) ? dict["LEAGUE_INDEX"] : null;
            console.log("LOAD_GAMES, sport = ", sport, " leagueIndex = ", leagueIndex);
            
            api.getGames(
                sport, 
                leagueIndex, // Pass the new folder index into the API!
                function(games) {
                    comms.sendGameList(requestID, games);
                },
                function() {
                    comms.sendGameListError(requestID);
                }
            );
            break;

        case ("UPDATE_GAME_ID" in dict):
            const game_id = dict["UPDATE_GAME_ID"]
            const game_sport = dict["UPDATE_GAME_SPORT"]
            console.log("Updating game id = ", game_id, ", sport = ", game_sport)
            api.getGame(
                game_id.toString(), game_sport,
                function(game) {
                    comms.sendGameUpdate(requestID, game);
                },
                function() {
                    comms.sendGameUpdateError(requestID);
                }
                )
            break;

        case ("ADD_FAVORITE_SPORT" in dict):
            const favoriteSport = dict["ADD_FAVORITE_SPORT"];
            const favoriteTeamID = dict["ADD_FAVORITE_TEAM_ID"];
            const favoriteTeam = new models.FavoriteTeam(favoriteSport, favoriteTeamID.toString());
            const added = storage.updateFavorite(favoriteTeam);
            comms.sendFavoritesResult(requestID, added);

            // AUTOMATED TIMELINE PUSH:
            // This force-triggers the API to fetch current games for all favorites 
            // and pushes them to the timeline immediately.
            console.log("Favorite changed: Triggering background timeline refresh.");
            api.getGames(models.sports.FAVORITES, null, 
                function(games) { 
                    console.log("Timeline background push triggered by favorite update."); 
                }, 
                function(error) { 
                    console.log("Timeline background push failed."); 
                }
            );
            break;
    }
});