CC = gcc
CFLAGS = -g -Wall -Wvla -std=c99 -fsanitize=address,undefined
DEBUG_OBJS = debug_nim.o
REGULAR_OBJS = nim.o


regular: $(REGULAR_OBJS)
	$(CC) $(CFLAGS) $^ -o nim

debug: $(DEBUG_OBJS)
	$(CC) $(CFLAGS) $^ -o debug_nim

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o nim debug_nim