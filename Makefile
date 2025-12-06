CC = gcc
CFLAGS = -g -Wall -Wvla -std=c99 -fsanitize=address,undefined
DEBUG_OBJS = debug_nim.o
REGULAR_OBJS = nim.o decoder.o game.o


regular: $(REGULAR_OBJS)
	$(CC) $(CFLAGS) $^ -o nimd

debug: $(DEBUG_OBJS)
	$(CC) $(CFLAGS) $^ -o debug_nim

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

nim.o: decoder.h game.h
decoder.o: decoder.h decoder.c

clean:
	rm -f *.o nimd debug_nim