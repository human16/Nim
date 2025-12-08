#define _POSIX_C_SOURCE 200809L
#include "decoder.h"
#include "game.h"
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

void playGame(Player *p1, Player *p2) {
  Game game;
  init_game(&game);

  p1->playing = 1;
  p2->playing = 1;

  send_play(&game);
  

  // Game loop and logic would go here
}

void init_game(Game *g) {
  // default config of 1, 3, 5, 7, 9
  for (int i = 0; i < 5; i++) {
    g->piles[i] = i * 2 + 1;
  }
  g->curr_player = 1;
}

void send_play(Game *g) {
  char board[64];
  sprintf(board, "%d %d %d %d %d", g->piles[0], g->piles[1], g->piles[2], g->piles[3], g->piles[4]);
  
  char turn[8];
  sprintf(turn, "%d", g->curr_player);

  char *buf;
  int len = encode_message(buf, BUFLEN, "PLAY", turn, board);
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
    char buf[BUFLEN];
    int len = encode_fail(buf, sizeof(buf), ERR_INVALID);
    write(p->sock, buf, len);
    return -1;
  }
  if (bytes == 0) {
    return 0;
  }

  // game not open yet
  if (strcmp(msg.type, "OPEN") != 0) {
    char buf[BUFLEN];
    int len = encode_fail(buf, sizeof(buf), ERR_INVALID);
    write(p->sock, buf, len);
    return -1;
  }

  // already opened
  if (p->opened) {
    char buf[BUFLEN];
    int len = encode_fail(buf, sizeof(buf), ERR_ALREADY_OPEN);
    write(p->sock, buf, len);
    return -1;
  }

  // invalid name
  if (msg.fields[0] == NULL || strlen(msg.fields[0]) == 0 ||
      strlen(msg.fields[0]) > 72) {
    char buf[BUFLEN];
    int len = encode_fail(buf, sizeof(buf), ERR_INVALID);
    write(p->sock, buf, len);
    return -1;
  }

  strncpy(p->name, msg.fields[0], sizeof(p->name) - 1);
  p->name[sizeof(p->name) - 1] = '\0';
  p->opened = 1;

  memmove(p->buffer, p->buffer + bytes, p->buffer_size - bytes);
  p->buffer_size -= bytes;

  send_wait(p->sock);
  return 1;
}

int apply_move(Game *g, int pile, int count) { return EXIT_SUCCESS; }

int is_game_over(Game *g) {
  for (int i = 0; i < 5; i++) {
    if (g->piles[i] > 0) {
      return 0;
    }
  }
  return 1;
}
