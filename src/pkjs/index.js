var models = require('./models');
var storage = require('./storage');
var comms = require('./comms');
var api = require('./api');

var Clay = require('@rebble/clay');
var clayConfig = require('./config.json');
var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

Pebble.addEventListener('showConfiguration', function(e) {
  // Update the options for the favorites checkboxes dynamically based on storage
  var currentFavs = storage.storedFavorites();
  var favOptions = [];
  var selectedFavs = [];

  // Try to populate names from localStorage if we saved them, or just use IDs
  var nameMap = storage.getFavoritesNames();

  for (var i = 0; i < currentFavs.length; i++) {
      var fav = currentFavs[i];
      var key = fav.sport + ":" + fav.teamID;
      var name = nameMap[key] || ("Team " + fav.teamID + " (Sport " + fav.sport + ")");
      favOptions.push({ label: name, value: key });
      selectedFavs.push(key);
  }

  // Find the favorites checkbox group and update its options and default value
  for (var i = 0; i < clay.config.length; i++) {
      if (clay.config[i].type === 'section') {
          for (var j = 0; j < clay.config[i].items.length; j++) {
              if (clay.config[i].items[j].messageKey === 'CURRENT_FAVORITES') {
                  clay.config[i].items[j].options = favOptions;
                  break;
              }
          }
      }
  }

  // Update clay settings memory so checkboxes show as selected
  var settings = {};
  try { settings = JSON.parse(localStorage.getItem('clay-settings')) || {}; } catch (e) {}
  settings['CURRENT_FAVORITES'] = selectedFavs;
  localStorage.setItem('clay-settings', JSON.stringify(settings));

  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e && !e.response) {
    return;
  }

  var dict = clay.getSettings(e.response);
  console.log("Settings changed: " + JSON.stringify(dict));

  // 1. Process CURRENT_FAVORITES to see what was unchecked and remove them
  var currentFavs = storage.storedFavorites();
  var rawSettings = JSON.parse(decodeURIComponent(e.response));
  var keptFavs = rawSettings['CURRENT_FAVORITES'] ? rawSettings['CURRENT_FAVORITES'] : [];

  if (!Array.isArray(keptFavs)) {
      keptFavs = [keptFavs];
  }

  var newFavorites = [];
  var nameMap = storage.getFavoritesNames();

  for (var i = 0; i < currentFavs.length; i++) {
      var fav = currentFavs[i];
      var key = fav.sport + ":" + fav.teamID;
      if (keptFavs.indexOf(key) !== -1) {
          newFavorites.push(fav);
      } else {
          // It was removed
          delete nameMap[key];
      }
  }

  delete dict['CURRENT_FAVORITES'];

  // Workaround for Clay appending keys even if we delete them from dict.
  // Clay stores its settings in localStorage 'clay-settings' using the message keys.
  // When we hit save, the raw response string often includes the dynamic keys and sends it directly via sendAppMessage if we aren't careful,
  // or it errors out if the keys aren't in messageKeys array.

  var messageKeys = require('message_keys');
  if (messageKeys.CURRENT_FAVORITES !== undefined) delete dict[messageKeys.CURRENT_FAVORITES];

  // Push standard settings to Pebble watch (leagues and display options)
  Pebble.sendAppMessage(dict, function() {
      console.log('Sent config data to Pebble');
  }, function(err) {
      console.log('Failed to send config data: ' + JSON.stringify(err));
  });

  // Finalize favorites and trigger push
  storage.saveFavoritesNames(nameMap);
  localStorage.setItem('favorites', JSON.stringify(newFavorites));
  storage.storedFavorites(true);

  api.getGames(models.sports.FAVORITES, null,
      function() { console.log("Timeline background push triggered by settings update."); },
      function() { console.log("Timeline background push failed."); }
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

            if (added) {
                // We added from watch, we dont have the name, but save ID so it shows up
                var nameMap = storage.getFavoritesNames();
                nameMap[favoriteSport + ":" + favoriteTeamID.toString()] = "Team " + favoriteTeamID + " (Sport " + favoriteSport + ")";
                storage.saveFavoritesNames(nameMap);
            } else {
                // We removed from watch
                var nameMap = storage.getFavoritesNames();
                delete nameMap[favoriteSport + ":" + favoriteTeamID.toString()];
                storage.saveFavoritesNames(nameMap);
            }


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
})
