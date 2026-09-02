CC = gcc
CFLAGS = -I mocks -I ../src/c/data/model -I unity -I ../src/c/data/comms/favorites
SRC_MODELS = unity/unity.c ../src/c/data/model/models.c test_models.c
SRC_FAVORITES = unity/unity.c mocks/pebble.c ../src/c/data/model/models.c ../src/c/data/comms/favorites/favorites-handler.c test_favorites_handler.c

all: test_models test_favorites

test_models: $(SRC_MODELS)
	$(CC) $(CFLAGS) $(SRC_MODELS) -o test_models

test_favorites: $(SRC_FAVORITES)
	$(CC) $(CFLAGS) $(SRC_FAVORITES) -o test_favorites

test: test_models test_favorites
	./test_models
	./test_favorites

clean:
	rm -f test_models test_favorites
