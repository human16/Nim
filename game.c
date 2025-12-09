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

void init_game(Game *g) {
  // default config of 1, 3, 5, 7, 9
  for (int i = 0; i < 5; i++) {
    g->piles[i] = i * 2 + 1;
  }
  g->curr_player = 1;
}

int is_game_over(Game *g) {
  for (int i = 0; i < 5; i++) {
    if (g->piles[i] > 0) {
      return 0;
    }
  }
  return 1;
}

int do_move(Game *game, int pile, int count) {
  if (pile < 0 || pile >= 5) {
    return ERR_PILE_INDEX;
  }
  if (count <= 0 || count > game->piles[pile]) {
    return ERR_QUANTITY;
  }
  game->piles[pile] -= count;

  if (game->curr_player == 1) {
    game->curr_player = 2;
  } else {
    game->curr_player = 1;
  }
  return ERR_NONE;
}

void send_msg(int fd, const char *msg, int len) {
  int sent = 0;
  while (sent < len) {
    int n = write(fd, msg + sent, len - sent);
    if (n <= 0) {
      return;
    }
    sent += n;
  }
}

void send_name(Player *p1, Player *p2) {
  char buf1[BUFLEN];
  int len1 = encode_message(buf1, BUFLEN, "NAME", "1", p2->name);
  if (len1 > 0) {
    printf("Sending NAME to P1\n");
    send_msg(p1->sock, buf1, len1);
  }

  char buf2[BUFLEN];
  int len2 = encode_message(buf2, BUFLEN, "NAME", "2", p1->name);
  if (len2 > 0) {
    printf("Sending NAME to P2\n");
    send_msg(p2->sock, buf2, len2);
  }
}

void send_wait(int sock) {
  char buf[BUFLEN];
  int len = encode_message(buf, BUFLEN, "WAIT");
  if (write(sock, buf, len) < 0) {
    perror("write");
  }
}

void send_over(Game *g, Player *p1, Player *p2, int winner, int forfeit) {
  char winner_str[8];
  sprintf(winner_str, "%d", winner);

  char board[64];
  sprintf(board, "%d %d %d %d %d", g->piles[0], g->piles[1], g->piles[2],
          g->piles[3], g->piles[4]);

  char buf[BUFLEN];
  int len;
  if (forfeit) {
    len = encode_message(buf, BUFLEN, "OVER", winner_str, board, "Forfeit");
  } else {
    len = encode_message(buf, BUFLEN, "OVER", winner_str, board, "");
  }

  if (len > 0) {
    if (p1 != NULL) {
      send_msg(p1->sock, buf, len);
    }
    if (p2 != NULL) {
      send_msg(p2->sock, buf, len);
    }
    printf("Game over.\n");
  }
}

void send_play(Player *p1, Player *p2, Game *g) {
  char board[64];
  sprintf(board, "%d %d %d %d %d", g->piles[0], g->piles[1], g->piles[2],
          g->piles[3], g->piles[4]);

  char turn[8];
  sprintf(turn, "%d", g->curr_player);

  char buf[BUFLEN];
  int len = encode_message(buf, BUFLEN, "PLAY", turn, board);

  if (len > 0) {
    printf("Sending PLAY\n");
    send_msg(p1->sock, buf, len);
    send_msg(p2->sock, buf, len);
  }
}

int openGame(Player *p) {
  Message msg;
  int bytes = decode_message(p->buffer, p->buffer_size, &msg);

  if (bytes < 0) {
    char buf[BUFLEN];
    int len = encode_fail(buf, sizeof(buf), msg.error_code);
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

  printf("Player %s opened a game.\n", p->name);
  memmove(p->buffer, p->buffer + bytes, p->buffer_size - bytes);
  p->buffer_size -= bytes;

  send_wait(p->sock);
  return 1;
}

void playGame(Player *p1, Player *p2) {
  Game game;
  init_game(&game);

  p1->playing = 1;
  p2->playing = 1;

  printf("sending names\n");
  send_name(p1, p2);

  send_play(p1, p2, &game);

  while (!is_game_over(&game)) {
    Player *current;
    Player *waiting;
    if (game.curr_player == 1) {
      current = p1;
      waiting = p2;
    } else {
      current = p2;
      waiting = p1;
    }

    int bytes = read(current->sock, current->buffer + current->buffer_size,
                     BUFLEN - current->buffer_size);

    if (bytes < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return;
      }
      printf("Player %d disconnected\n", game.curr_player);
      send_over(&game, waiting, NULL, waiting->p_num, 1);
      return;
    }

    if (bytes == 0) {
      printf("Player %d disconnected\n", game.curr_player);
      send_over(&game, waiting, NULL, waiting->p_num, 1);
      return;
    }

    current->buffer_size += bytes;

    Message msg;
    int msg_bytes = decode_message(current->buffer, current->buffer_size, &msg);

    if (msg_bytes < 0) {
      printf("Invalid message\n");
      char buf[256];
      int len = encode_fail(buf, sizeof(buf), ERR_INVALID);
      if (len > 0) {
        send_msg(current->sock, buf, len);
      }
      close(current->sock);
    }

    if (msg_bytes == 0) {
      continue;
    }

    memmove(current->buffer, current->buffer + msg_bytes,
            current->buffer_size - msg_bytes);
    current->buffer_size -= msg_bytes;

    if (strcmp(msg.type, "MOVE") != 0) {
      printf("Expected MOVE message\n");
      char buf[256];
      int len = encode_fail(buf, sizeof(buf), ERR_INVALID);
      if (len > 0) {
        send_msg(current->sock, buf, len);
      }
      continue;
    }

    int pile = atoi(msg.fields[0]);
    int count = atoi(msg.fields[1]);

    printf("Player %d MOVE pile %d count %d\n", game.curr_player, pile, count);

    int err = do_move(&game, pile, count);

    if (err != ERR_NONE) {
      printf("Invalid move\n");
      char buf[256];
      int len = encode_fail(buf, sizeof(buf), err);
      if (len > 0) {
        send_msg(current->sock, buf, len);
      }
      continue;
    }

    if (is_game_over(&game)) {
      printf("OVER sent\n");
      send_over(&game, current, waiting, current->p_num, 0);
      return;
    } else {
      send_play(p1, p2, &game);
    }
  }

  // Game loop and logic would go here
}
