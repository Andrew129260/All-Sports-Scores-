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
  var nameMapStr = localStorage.getItem('favoritesNames');
  var nameMap = {};
  if (nameMapStr) {
      try { nameMap = JSON.parse(nameMapStr); } catch(e){}
  }

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
  // Make sure to clear previous search
  settings['FAVORITE_TEAM_SEARCH'] = "";
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
  var keptFavs = dict['CURRENT_FAVORITES'] || [];
  if (!Array.isArray(keptFavs)) {
      keptFavs = [keptFavs];
  }

  var newFavorites = [];
  var nameMapStr = localStorage.getItem('favoritesNames');
  var nameMap = {};
  if (nameMapStr) { try { nameMap = JSON.parse(nameMapStr); } catch(e){} }

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

  var searchQuery = dict['FAVORITE_TEAM_SEARCH'];
  delete dict['FAVORITE_TEAM_SEARCH'];
  delete dict['CURRENT_FAVORITES'];

  // Push standard settings to Pebble watch (leagues and display options)
  Pebble.sendAppMessage(dict, function() {
      console.log('Sent config data to Pebble');
  }, function(err) {
      console.log('Failed to send config data: ' + JSON.stringify(err));
  });

  // Function to finalize favorites and trigger push
  function finalizeFavorites(favorites, newNameMap) {
      localStorage.setItem('favoritesNames', JSON.stringify(newNameMap));
      localStorage.setItem('favorites', JSON.stringify(favorites));
      storage.storedFavorites(true);

      api.getGames(models.sports.FAVORITES, null,
          function() { console.log("Timeline background push triggered by settings update."); },
          function() { console.log("Timeline background push failed."); }
      );
  }

  // 2. Process search query if any
  if (searchQuery && searchQuery.trim().length > 0) {
      var query = searchQuery.trim();
      var url = "https://site.web.api.espn.com/apis/search/v2?region=us&lang=en&query=" + encodeURIComponent(query) + "&limit=5&type=team";
      var req = new XMLHttpRequest();
      req.open('GET', url, true);
      req.onload = function() {
          if (req.status == 200) {
              var data = JSON.parse(req.responseText);
              if (data.results && data.results.length > 0 && data.results[0].contents && data.results[0].contents.length > 0) {
                  var bestMatch = data.results[0].contents[0];


                  // Map sport name to ID
                  var sportId = -1;
                  var sportStr = bestMatch.sport;
                  if (sportStr === "football" && bestMatch.defaultLeagueSlug === "nfl") sportId = models.sports.NFL;
                  else if (sportStr === "baseball" && bestMatch.defaultLeagueSlug === "mlb") sportId = models.sports.MLB;
                  else if (sportStr === "hockey" && bestMatch.defaultLeagueSlug === "nhl") sportId = models.sports.NHL;
                  else if (sportStr === "basketball" && bestMatch.defaultLeagueSlug === "nba") sportId = models.sports.NBA;
                  else if (sportStr === "soccer") sportId = models.sports.MLS; // Note: We map all soccer to MLS for this app currently
                  else if (sportStr === "australian-football") sportId = models.sports.AFL;
                  else if (sportStr === "cricket") sportId = models.sports.CRICKET;
                  else if (sportStr === "rugby-league" || sportStr === "rugby-union") sportId = models.sports.RUGBY;

                  if (sportId !== -1) {
                      var teamIdStr = bestMatch.uid.split('~t:')[1];
                      if (teamIdStr) {
                          var newTeam = new models.FavoriteTeam(sportId, teamIdStr);
                          var key = sportId + ":" + teamIdStr;

                          // Check if already in favorites
                          var exists = newFavorites.some(function(f) { return f.sport == sportId && f.teamID == teamIdStr; });
                          if (!exists) {
                              newFavorites.push(newTeam);
                              nameMap[key] = bestMatch.displayName;
                              console.log("Added new favorite from search: " + bestMatch.displayName);
                          } else {
                              console.log("Team already in favorites.");
                          }
                      }
                  } else {
                      console.log("Found team, but sport not supported.");
                  }
              } else {
                  console.log("No team found for query.");
              }
          }
          finalizeFavorites(newFavorites, nameMap);
      };
      req.onerror = function() {
          finalizeFavorites(newFavorites, nameMap);
      };
      req.send();
  } else {
      finalizeFavorites(newFavorites, nameMap);
  }
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
                var nameMapStr = localStorage.getItem('favoritesNames');
                var nameMap = {};
                if (nameMapStr) { try { nameMap = JSON.parse(nameMapStr); } catch(e){} }
                nameMap[favoriteSport + ":" + favoriteTeamID.toString()] = "Team " + favoriteTeamID + " (Sport " + favoriteSport + ")";
                localStorage.setItem('favoritesNames', JSON.stringify(nameMap));
            } else {
                // We removed from watch
                var nameMapStr = localStorage.getItem('favoritesNames');
                var nameMap = {};
                if (nameMapStr) { try { nameMap = JSON.parse(nameMapStr); } catch(e){} }
                delete nameMap[favoriteSport + ":" + favoriteTeamID.toString()];
                localStorage.setItem('favoritesNames', JSON.stringify(nameMap));
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
});