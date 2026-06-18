#include <stdbool.h>

#ifndef MODELS_H_
#define MODELS_H_

typedef enum {
    GamesListItem, GamesListLastItem, GamesListNoGames, GamesListNetworkError
} GamesListState;

typedef enum {
    GameUpdated, GameUpdateNetworkError
} GameUpdateResult;

typedef enum {
    ConnectionError, NetworkError, NoGames
} AppError;

// THE FIX: Perfectly matched to the JavaScript enumeration
typedef enum {
    Favorites = 0, 
    SportNFL = 1, 
    SportMLB = 2, 
    SportNHL = 3, 
    SportNBA = 4, 
    SportMLS = 5, 
    SportRugby = 6, 
    SportCricket = 7, 
    SportTennis = 8, 
    SportAFL = 9, 
    SportMMA = 10
} Sport;

typedef enum {
    Team1, Team2, None,
} Possession;

typedef struct Team {
    char *name;
    char *score;
    char *record;
    int id;
    bool favorite;
} Team;

typedef struct Game
{
    int id;
    Sport sport;
    char* league; // Memory slot for the league header!
    Team team1;
    Team team2;
    Possession possession;
    char* time;
    char* details;
    char* summary;
    char* broadcast;
} Game;

int sport_get_icon_res_small(Sport sport);
int sport_get_icon_res_large(Sport sport);
char* sport_get_name(Sport sport);

#endif
