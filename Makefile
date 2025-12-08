CC = gcc
CFLAGS = -g -Wall -Wvla -std=c99 -fsanitize=address,undefined
DEBUG_OBJS = debug_nim.o
REGULAR_OBJS = nim.o decoder.o game.o
TEST_DECODER_OBJS = test_decoder.o decoder.o
TEST_GAME_OBJS = test_game.o game.o decoder.o


regular: $(REGULAR_OBJS)
	$(CC) $(CFLAGS) $^ -o nimd

debug: $(DEBUG_OBJS)
	$(CC) $(CFLAGS) $^ -o debug_nim

test_decoder: $(TEST_DECODER_OBJS)
	$(CC) $(CFLAGS) $^ -o test_decoder
# ./test_decoder

test_game: $(TEST_GAME_OBJS)
	$(CC) $(CFLAGS) $^ -o test_game
# ./test_game

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

nim.o: decoder.h game.h
decoder.o: decoder.h decoder.c

clean:
	rm -f *.o nimd debug_nim test_decoder test_game
