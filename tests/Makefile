CC = gcc
CFLAGS = -I mocks -I ../src/c/data/model -I unity
SRC = unity/unity.c ../src/c/data/model/models.c test_models.c

all: test_models

test_models: $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o test_models

test: test_models
	./test_models

clean:
	rm -f test_models
