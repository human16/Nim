#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "decoder.h"
#include "game.h"

static int tests_passed = 0;
static int tests_failed = 0;

static void assert_test(int condition, const char *test_name,
                        const char *message) {
  if (condition) {
    printf("  PASS: %s\n", test_name);
    tests_passed++;
  } else {
    printf("  FAIL: %s - %s\n", test_name, message);
    tests_failed++;
  }
}

/* create a Player struct with a socketpair for testing.
 * Returns the other end socket that simulates the network peer.
 * Caller must free p->buffer and close both sockets. */
static int create_test_player(Player *p, int p_num) {
  int sv[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
    perror("socketpair");
    return -1;
  }

  p->sock = sv[0];
  p->name[0] = '\0';
  p->p_num = p_num;
  p->opened = 0;
  p->buffer = malloc(BUFLEN);
  p->buffer_size = 0;
  p->playing = 0;

  return sv[1];
}

static void cleanup_test_player(Player *p, int peer_sock) {
  if (p->buffer)
    free(p->buffer);
  if (p->sock >= 0)
    close(p->sock);
  if (peer_sock >= 0)
    close(peer_sock);
}

static int read_response(int peer_sock, char *buf, int bufsize) {
  int flags = fcntl(peer_sock, F_GETFL, 0);
  fcntl(peer_sock, F_SETFL, flags | O_NONBLOCK);

  int n = read(peer_sock, buf, bufsize - 1);
  if (n > 0)
    buf[n] = '\0';

  fcntl(peer_sock, F_SETFL, flags);
  return n;
}

void test_init_game() {
  printf("\n--- init_game() Tests ---\n");

  /* verify initial pile configuration */
  {
    Game g;
    memset(&g, 0xFF, sizeof(g));
    init_game(&g);

    int pass = (g.piles[0] == 1 && g.piles[1] == 3 && g.piles[2] == 5 &&
                g.piles[3] == 7 && g.piles[4] == 9);

    assert_test(pass, "init_piles", "Should initialize piles to 1,3,5,7,9");
  }

  /* verify initial current player */
  {
    Game g;
    init_game(&g);

    int pass = (g.curr_player == 1);

    assert_test(pass, "init_curr_player", "Should set curr_player to 1");
  }

  /* multiple init calls should reset state */
  {
    Game g;
    init_game(&g);

    g.piles[0] = 0;
    g.piles[1] = 0;
    g.curr_player = 2;

    init_game(&g);

    int pass = (g.piles[0] == 1 && g.piles[1] == 3 && g.curr_player == 1);

    assert_test(pass, "init_reset", "Should reset state on re-init");
  }

  /* verify total stone count */
  {
    Game g;
    init_game(&g);

    int total = 0;
    for (int i = 0; i < 5; i++) {
      total += g.piles[i];
    }

    int pass = (total == 25);

    assert_test(pass, "init_total_stones", "Total stones should be 25");
  }
}

void test_is_game_over() {
  printf("\n--- is_game_over() Tests ---\n");

  /* game not over at start */
  {
    Game g;
    init_game(&g);

    int pass = (is_game_over(&g) == 0);

    assert_test(pass, "not_over_initial", "Game should not be over at start");
  }

  /* game over when all piles empty */
  {
    Game g;
    init_game(&g);

    for (int i = 0; i < 5; i++) {
      g.piles[i] = 0;
    }

    int pass = (is_game_over(&g) == 1);

    assert_test(pass, "over_all_empty",
                "Game should be over when all piles empty");
  }

  /* game not over with one stone remaining */
  {
    Game g;
    init_game(&g);

    g.piles[0] = 0;
    g.piles[1] = 0;
    g.piles[2] = 1;
    g.piles[3] = 0;
    g.piles[4] = 0;

    int pass = (is_game_over(&g) == 0);

    assert_test(pass, "not_over_one_stone",
                "Game should not be over with 1 stone");
  }

  /* game not over with stones in last pile only */
  {
    Game g;
    init_game(&g);

    g.piles[0] = 0;
    g.piles[1] = 0;
    g.piles[2] = 0;
    g.piles[3] = 0;
    g.piles[4] = 5;

    int pass = (is_game_over(&g) == 0);

    assert_test(pass, "not_over_last_pile",
                "Game should not be over with stones in pile 4");
  }

  /* game not over with stones in first pile only */
  {
    Game g;
    init_game(&g);

    g.piles[0] = 1;
    g.piles[1] = 0;
    g.piles[2] = 0;
    g.piles[3] = 0;
    g.piles[4] = 0;

    int pass = (is_game_over(&g) == 0);

    assert_test(pass, "not_over_first_pile",
                "Game should not be over with stones in pile 0");
  }

  /* large pile values shouldn't break check */
  {
    Game g;
    init_game(&g);

    g.piles[0] = 1000000;

    int pass = (is_game_over(&g) == 0);

    assert_test(pass, "large_value", "Large pile value should not break check");
  }
}

void test_apply_move() {
  printf("\n--- apply_move() Tests ---\n");

  /* basic valid move */
  {
    Game g;
    init_game(&g);

    int result = apply_move(&g, 1, 2);

    int pass = (result == 0 && g.piles[1] == 1);

    assert_test(pass, "basic_move",
                "Remove 2 from pile 1 (has 3), should leave 1");
  }

  /* remove all stones from a pile */
  {
    Game g;
    init_game(&g);

    int result = apply_move(&g, 2, 5);

    int pass = (result == 0 && g.piles[2] == 0);

    assert_test(pass, "remove_all",
                "Remove all 5 from pile 2, should be empty");
  }

  /* remove one stone */
  {
    Game g;
    init_game(&g);

    int result = apply_move(&g, 4, 1);

    int pass = (result == 0 && g.piles[4] == 8);

    assert_test(pass, "remove_one",
                "remove 1 from pile 4 (has 9), should leave 8");
  }

  /* invalid pile index (negative) */
  {
    Game g;
    init_game(&g);

    int result = apply_move(&g, -1, 1);

    int pass = (result != 0 && g.piles[0] == 1);

    assert_test(pass, "invalid_pile_negative",
                "Negative pile index should fail");
  }

  /* invalid pile index (too large) */
  {
    Game g;
    init_game(&g);

    int result = apply_move(&g, 5, 1);

    int pass = (result != 0);

    assert_test(pass, "invalid_pile_large",
                "Pile index 5 should fail (only 0-4 valid)");
  }

  /* remove zero stones (invalid) */
  {
    Game g;
    init_game(&g);

    int result = apply_move(&g, 2, 0);

    int pass = (result != 0 && g.piles[2] == 5);

    assert_test(pass, "zero_stones", "Removing 0 stones should fail");
  }

  /* remove more stones than available */
  {
    Game g;
    init_game(&g);

    int result = apply_move(&g, 0, 2);

    int pass = (result != 0 && g.piles[0] == 1);

    assert_test(pass, "too_many", "Removing more than available should fail");
  }

  /* remove from empty pile */
  {
    Game g;
    init_game(&g);
    g.piles[0] = 0;

    int result = apply_move(&g, 0, 1);

    int pass = (result != 0);

    assert_test(pass, "empty_pile", "Removing from empty pile should fail");
  }

  /* negative count */
  {
    Game g;
    init_game(&g);

    int result = apply_move(&g, 2, -1);

    int pass = (result != 0 && g.piles[2] == 5);

    assert_test(pass, "negative_count", "Negative count should fail");
  }

  /* sequential moves until game over */
  {
    Game g;
    init_game(&g);

    int pass = 1;

    if (apply_move(&g, 0, 1) != 0 || g.piles[0] != 0)
      pass = 0;
    if (apply_move(&g, 1, 3) != 0 || g.piles[1] != 0)
      pass = 0;
    if (apply_move(&g, 2, 5) != 0 || g.piles[2] != 0)
      pass = 0;
    if (apply_move(&g, 3, 7) != 0 || g.piles[3] != 0)
      pass = 0;
    if (is_game_over(&g) != 0)
      pass = 0;
    if (apply_move(&g, 4, 9) != 0 || g.piles[4] != 0)
      pass = 0;
    if (is_game_over(&g) != 1)
      pass = 0;

    assert_test(pass, "sequence_to_gameover",
                "Sequential moves should empty board");
  }
}

void test_openGame() {
  printf("\n--- openGame() Tests ---\n");

  /* valid OPEN message */
  {
    Player p;
    int peer = create_test_player(&p, 1);

    char msg[] = "0|11|OPEN|Alice|";
    memcpy(p.buffer, msg, sizeof(msg) - 1);
    p.buffer_size = sizeof(msg) - 1;

    int result = openGame(&p);

    char resp[BUFLEN];
    int n = read_response(peer, resp, sizeof(resp));

    int pass = (result == 1 && p.opened == 1 && strcmp(p.name, "Alice") == 0 &&
                n > 0 && strstr(resp, "WAIT") != NULL);

    assert_test(pass, "valid_open", "Should accept valid OPEN and send WAIT");

    cleanup_test_player(&p, peer);
  }

  /* OPEN with max length name (72 chars) */
  {
    Player p;
    int peer = create_test_player(&p, 1);

    char name72[73];
    memset(name72, 'A', 72);
    name72[72] = '\0';

    char msg[128];
    int len = snprintf(msg, sizeof(msg), "0|78|OPEN|%s|", name72);

    memcpy(p.buffer, msg, len);
    p.buffer_size = len;

    int result = openGame(&p);

    int pass = (result == 1 && strlen(p.name) == 72);

    assert_test(pass, "max_name_72", "Should accept 72-char name");

    cleanup_test_player(&p, peer);
  }

  /* OPEN with name too long (73+ chars) */
  {
    Player p;
    int peer = create_test_player(&p, 1);

    char name73[74];
    memset(name73, 'B', 73);
    name73[73] = '\0';

    char msg[128];
    int len = snprintf(msg, sizeof(msg), "0|79|OPEN|%s|", name73);

    memcpy(p.buffer, msg, len);
    p.buffer_size = len;

    int result = openGame(&p);

    int pass = (result == -1 && p.opened == 0);

    assert_test(pass, "name_too_long", "Should reject 73-char name");

    cleanup_test_player(&p, peer);
  }

  /* OPEN with empty name */
  {
    Player p;
    int peer = create_test_player(&p, 1);

    char msg[] = "0|06|OPEN||";
    memcpy(p.buffer, msg, sizeof(msg) - 1);
    p.buffer_size = sizeof(msg) - 1;

    int result = openGame(&p);

    int pass = (result == -1 && p.opened == 0);

    assert_test(pass, "empty_name", "Should reject empty name");

    cleanup_test_player(&p, peer);
  }

  /* non-OPEN message before opening */
  {
    Player p;
    int peer = create_test_player(&p, 1);

    char msg[] = "0|09|MOVE|1|2|";
    memcpy(p.buffer, msg, sizeof(msg) - 1);
    p.buffer_size = sizeof(msg) - 1;

    int result = openGame(&p);

    int pass = (result == -1 && p.opened == 0);

    assert_test(pass, "wrong_message", "Should reject non-OPEN message");

    cleanup_test_player(&p, peer);
  }

  /* double OPEN (already opened) */
  {
    Player p;
    int peer = create_test_player(&p, 1);

    char msg1[] = "0|09|OPEN|Bob|";
    memcpy(p.buffer, msg1, sizeof(msg1) - 1);
    p.buffer_size = sizeof(msg1) - 1;

    int result1 = openGame(&p);

    char resp[BUFLEN];
    read_response(peer, resp, sizeof(resp));

    char msg2[] = "0|10|OPEN|Bob2|";
    memcpy(p.buffer, msg2, sizeof(msg2) - 1);
    p.buffer_size = sizeof(msg2) - 1;

    int result2 = openGame(&p);

    int n = read_response(peer, resp, sizeof(resp));

    int pass = (result1 == 1 && result2 == -1 && strcmp(p.name, "Bob") == 0 &&
                n > 0 && strstr(resp, "Already Open") != NULL);

    assert_test(pass, "already_opened",
                "Second OPEN should fail with Already Open");

    cleanup_test_player(&p, peer);
  }

  /* incomplete message (need more data) */
  {
    Player p;
    int peer = create_test_player(&p, 1);

    char msg[] = "0|11|OPEN|Ali";
    memcpy(p.buffer, msg, sizeof(msg) - 1);
    p.buffer_size = sizeof(msg) - 1;

    int result = openGame(&p);

    int pass = (result == 0 && p.opened == 0 &&
                p.buffer_size == (int)(sizeof(msg) - 1));

    assert_test(pass, "incomplete", "Incomplete message should return 0");

    cleanup_test_player(&p, peer);
  }

  /* invalid message format */
  {
    Player p;
    int peer = create_test_player(&p, 1);

    char msg[] = "1|11|OPEN|Alice|";
    memcpy(p.buffer, msg, sizeof(msg) - 1);
    p.buffer_size = sizeof(msg) - 1;

    int result = openGame(&p);

    int pass = (result == -1);

    assert_test(pass, "invalid_format", "Invalid version should fail");

    cleanup_test_player(&p, peer);
  }

  /* buffer consumption after successful OPEN */
  {
    Player p;
    int peer = create_test_player(&p, 1);

    char msg[] = "0|10|OPEN|Test|EXTRA";
    memcpy(p.buffer, msg, sizeof(msg) - 1);
    p.buffer_size = sizeof(msg) - 1;

    int result = openGame(&p);

    int pass = (result == 1 && p.buffer_size == 5 &&
                strncmp(p.buffer, "EXTRA", 5) == 0);

    assert_test(pass, "buffer_consumed",
                "Should consume message and leave extra data");

    cleanup_test_player(&p, peer);
  }

  /* name with spaces */
  {
    Player p;
    int peer = create_test_player(&p, 1);

    char msg[] = "0|17|OPEN|John Doe Jr|";
    memcpy(p.buffer, msg, sizeof(msg) - 1);
    p.buffer_size = sizeof(msg) - 1;

    int result = openGame(&p);

    int pass = (result == 1 && strcmp(p.name, "John Doe Jr") == 0);

    assert_test(pass, "name_with_spaces", "Should accept name with spaces");

    cleanup_test_player(&p, peer);
  }

  /* name with special characters */
  {
    Player p;
    int peer = create_test_player(&p, 1);

    char msg[] = "0|15|OPEN|Test-._@!|";
    memcpy(p.buffer, msg, sizeof(msg) - 1);
    p.buffer_size = sizeof(msg) - 1;

    int result = openGame(&p);

    int pass = (result == 1 && strcmp(p.name, "Test-._@!") == 0);

    assert_test(pass, "special_chars",
                "Should accept special chars (except |)");

    cleanup_test_player(&p, peer);
  }

  /* empty buffer */
  {
    Player p;
    int peer = create_test_player(&p, 1);

    p.buffer_size = 0;

    int result = openGame(&p);

    int pass = (result == 0 && p.opened == 0);

    assert_test(pass, "empty_buffer", "Empty buffer should return 0");

    cleanup_test_player(&p, peer);
  }

  /* name with pipe character, forbidden by p4 spec */
  {
    Player p;
    int peer = create_test_player(&p, 1);

    char msg[] = "0|14|OPEN|Bad|Name|";
    memcpy(p.buffer, msg, sizeof(msg) - 1);
    p.buffer_size = sizeof(msg) - 1;

    int result = openGame(&p);

    int pass = (result == -1);

    assert_test(pass, "name_with_pipe",
                "Should reject name containing pipe character");

    cleanup_test_player(&p, peer);
  }

  /* WAIT message format verification */
  {
    Player p;
    int peer = create_test_player(&p, 1);

    char msg[] = "0|11|OPEN|Alice|";
    memcpy(p.buffer, msg, sizeof(msg) - 1);
    p.buffer_size = sizeof(msg) - 1;

    int result = openGame(&p);

    char resp[BUFLEN];
    int n = read_response(peer, resp, sizeof(resp));

    int pass = (result == 1 && n > 0 && strcmp(resp, "0|05|WAIT|") == 0);

    assert_test(pass, "wait_format", "Should send exact WAIT message format");

    cleanup_test_player(&p, peer);
  }

  /* FAIL message for name too long - verify error code */
  {
    Player p;
    int peer = create_test_player(&p, 1);

    char name73[74];
    memset(name73, 'B', 73);
    name73[73] = '\0';

    char msg[128];
    int len = snprintf(msg, sizeof(msg), "0|79|OPEN|%s|", name73);

    memcpy(p.buffer, msg, len);
    p.buffer_size = len;

    int result = openGame(&p);

    char resp[BUFLEN];
    int n = read_response(peer, resp, sizeof(resp));

    int pass = (result == -1 && n > 0 && strstr(resp, "21") != NULL &&
                strstr(resp, "Long Name") != NULL);

    assert_test(pass, "fail_long_name_code",
                "Should send FAIL with '21 Long Name'");

    cleanup_test_player(&p, peer);
  }

  /* FAIL message for already open */
  {
    Player p;
    int peer = create_test_player(&p, 1);

    char msg1[] = "0|09|OPEN|Bob|";
    memcpy(p.buffer, msg1, sizeof(msg1) - 1);
    p.buffer_size = sizeof(msg1) - 1;

    openGame(&p);

    char resp[BUFLEN];
    read_response(peer, resp, sizeof(resp));

    char msg2[] = "0|10|OPEN|Bob2|";
    memcpy(p.buffer, msg2, sizeof(msg2) - 1);
    p.buffer_size = sizeof(msg2) - 1;

    int result2 = openGame(&p);

    int n = read_response(peer, resp, sizeof(resp));

    int pass = (result2 == -1 && n > 0 && strstr(resp, "23") != NULL &&
                strstr(resp, "Already Open") != NULL);

    assert_test(pass, "fail_already_open_code",
                "Should send FAIL with '23 Already Open'");

    cleanup_test_player(&p, peer);
  }

  /* FAIL message for non-OPEN message */
  {
    Player p;
    int peer = create_test_player(&p, 1);

    char msg[] = "0|09|MOVE|1|2|";
    memcpy(p.buffer, msg, sizeof(msg) - 1);
    p.buffer_size = sizeof(msg) - 1;

    int result = openGame(&p);

    char resp[BUFLEN];
    int n = read_response(peer, resp, sizeof(resp));

    int pass = (result == -1 && n > 0 && strstr(resp, "FAIL") != NULL);

    assert_test(pass, "fail_wrong_message_type",
                "Should send FAIL for non-OPEN message");

    cleanup_test_player(&p, peer);
  }
}

void test_playGame() {
  printf("\n--- playGame() Tests ---\n");

  /* playGame sets playing flag */
  {
    Player p1, p2;
    int peer1 = create_test_player(&p1, 1);
    int peer2 = create_test_player(&p2, 2);

    strcpy(p1.name, "Player1");
    strcpy(p2.name, "Player2");
    p1.opened = 1;
    p2.opened = 1;

    playGame(&p1, &p2);

    int pass = (p1.playing == 1 && p2.playing == 1);

    assert_test(pass, "sets_playing", "Should mark both players as playing");

    cleanup_test_player(&p1, peer1);
    cleanup_test_player(&p2, peer2);
  }

  /* playGame sends NAME messages to both players */
  {
    Player p1, p2;
    int peer1 = create_test_player(&p1, 1);
    int peer2 = create_test_player(&p2, 2);

    strcpy(p1.name, "Alice");
    strcpy(p2.name, "Bob");
    p1.opened = 1;
    p2.opened = 1;

    playGame(&p1, &p2);

    char resp1[BUFLEN];
    char resp2[BUFLEN];
    int n1 = read_response(peer1, resp1, sizeof(resp1));
    int n2 = read_response(peer2, resp2, sizeof(resp2));

    int pass = (n1 > 0 && strstr(resp1, "NAME") != NULL &&
                strstr(resp1, "1") != NULL && strstr(resp1, "Bob") != NULL &&
                n2 > 0 && strstr(resp2, "NAME") != NULL &&
                strstr(resp2, "2") != NULL && strstr(resp2, "Alice") != NULL);

    assert_test(pass, "sends_name",
                "Should send NAME with player numbers and opponent names");

    cleanup_test_player(&p1, peer1);
    cleanup_test_player(&p2, peer2);
  }

  /* playGame sends initial PLAY message with starting board */
  {
    Player p1, p2;
    int peer1 = create_test_player(&p1, 1);
    int peer2 = create_test_player(&p2, 2);

    strcpy(p1.name, "Alice");
    strcpy(p2.name, "Bob");
    p1.opened = 1;
    p2.opened = 1;

    playGame(&p1, &p2);

    char resp1[BUFLEN];
    char resp2[BUFLEN];
    read_response(peer1, resp1, sizeof(resp1));
    read_response(peer2, resp2, sizeof(resp2));

    /* look for PLAY message with initial board state "1 3 5 7 9" */
    int pass =
        ((strstr(resp1, "PLAY") != NULL &&
          strstr(resp1, "1 3 5 7 9") != NULL) ||
         (strstr(resp2, "PLAY") != NULL && strstr(resp2, "1 3 5 7 9") != NULL));

    assert_test(pass, "sends_initial_play",
                "Should send PLAY with initial board state 1 3 5 7 9");

    cleanup_test_player(&p1, peer1);
    cleanup_test_player(&p2, peer2);
  }

  /* playGame indicates player 1 moves first */
  {
    Player p1, p2;
    int peer1 = create_test_player(&p1, 1);
    int peer2 = create_test_player(&p2, 2);

    strcpy(p1.name, "Alice");
    strcpy(p2.name, "Bob");
    p1.opened = 1;
    p2.opened = 1;

    playGame(&p1, &p2);

    char resp1[BUFLEN];
    char resp2[BUFLEN];
    read_response(peer1, resp1, sizeof(resp1));
    read_response(peer2, resp2, sizeof(resp2));

    /* PLAY message should indicate player 1's turn */
    int pass =
        ((strstr(resp1, "PLAY") != NULL && strstr(resp1, "|1|") != NULL) ||
         (strstr(resp2, "PLAY") != NULL && strstr(resp2, "|1|") != NULL));

    assert_test(pass, "player1_first",
                "PLAY message should indicate player 1 goes first");

    cleanup_test_player(&p1, peer1);
    cleanup_test_player(&p2, peer2);
  }
}

int main() {
  printf("==============================================\n");
  printf("   Game Module Test Suite\n");
  printf("==============================================\n");

  test_init_game();
  test_is_game_over();
  test_apply_move();
  test_openGame();
  test_playGame();

  printf("\n==============================================\n");
  printf("   Results: %d passed, %d failed\n", tests_passed, tests_failed);
  printf("==============================================\n");

  if (tests_failed > 0) {
    printf("\nNote: apply_move() tests are expected to fail\n");
    printf("\t\tuntil function is implemented.\n");
  }

  return (tests_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
