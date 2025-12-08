CC = gcc
CFLAGS = -g -Wall -Wvla -std=c99 -fsanitize=address,undefined
DEBUG_OBJS = debug_nim.o
REGULAR_OBJS = nim.o
TEST_OBJS = test_decoder.o decoder.o


regular: $(REGULAR_OBJS)
	$(CC) $(CFLAGS) $^ -o nimd

debug: $(DEBUG_OBJS)
	$(CC) $(CFLAGS) $^ -o debug_nim

test: $(TEST_OBJS)
	$(CC) $(CFLAGS) $^ -o test_decoder
	./test_decoder

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

nim.o: decoder.h game.h
decoder.o: decoder.h decoder.c

clean:
	rm -f *.o nim debug_nim test_decoder
