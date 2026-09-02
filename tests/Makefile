CC = gcc
CFLAGS = -I mocks -I ../src/c/data/model -I unity -I ../src/c/data/comms/favorites
SRC_MODELS = unity/unity.c ../src/c/data/model/models.c test_models.c
SRC_FAVORITES = unity/unity.c mocks/pebble.c ../src/c/data/model/models.c ../src/c/data/comms/favorites/favorites-handler.c test_favorites_handler.c

all: test_models test_favorites
CFLAGS = -I mocks -I ../src/c/data/model -I ../src/c/data/comms/games -I unity -g
SRC_MODELS = unity/unity.c ../src/c/data/model/models.c test_models.c
# We need to compile unity and the sources we are testing
SRC_GAMES = unity/unity.c ../src/c/data/model/models.c ../src/c/data/comms/games/games-handler.c test_games_handler.c

all: test_models test_games_handler

test_models: $(SRC_MODELS)
	$(CC) $(CFLAGS) $(SRC_MODELS) -o test_models

test_favorites: $(SRC_FAVORITES)
	$(CC) $(CFLAGS) $(SRC_FAVORITES) -o test_favorites

test: test_models test_favorites
	./test_models
	./test_favorites

clean:
	rm -f test_models test_favorites
test_games_handler: $(SRC_GAMES)
	$(CC) $(CFLAGS) $(SRC_GAMES) -o test_games_handler

test: test_models test_games_handler
	./test_models
	./test_games_handler

clean:
	rm -f test_models test_games_handler
