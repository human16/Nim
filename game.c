#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include "game.h"
#include "decoder.h"

void playGame(Player *p1, Player *p2) {
    Game game;
    //init_game(&game);

    // Game loop and logic would go here

}

void init_game(Game *g) {
    //todo
}

void send_wait(int sock) {
  char buf[BUFLEN];
  int len = encode_message(buf, BUFLEN, "WAIT");
  if (write(sock, buf, len) < 0) {
    perror("write");
  }
}

int openGame(Player *p) {
  Message msg;
  int bytes = decode_message(p->buffer, p->buffer_size, &msg);
  
  if (bytes < 0) {
    char *buf;
    int len = encode_fail(buf, sizeof(buf), ERR_INVALID);
    write(p->sock, buf, len);
    return -1;
  }
  if (bytes == 0) {
    return 0;
  }

  // game not open yet
  if (strcmp(msg.type, "OPEN") != 0) {
    char *buf;
    int len = encode_fail(buf, sizeof(buf), ERR_INVALID);
    write(p->sock, buf, len);
    return -1;
  }

  //already opened
  if (p->opened) {
    char *buf;
    int len = encode_fail(buf, sizeof(buf), ERR_ALREADY_OPEN);
    write(p->sock, buf, len);
    return -1;
  }

  strncpy(p->name, msg.fields[0], sizeof(p->name)-1);
  p->name[sizeof(p->name)-1] = '\0';
  p->opened = 1;
  
  memmove(p->buffer, p->buffer + bytes, p->buffer_size - bytes);
  p->buffer_size -= bytes;

  send_wait(p->sock);
  return 1;
}

int apply_move(Game *g, int pile, int count) {
    return EXIT_SUCCESS;
}

int is_game_over(Game *g) {
    return EXIT_SUCCESS;
}