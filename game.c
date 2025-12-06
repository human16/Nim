#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    int piles[5];
    int curr_player;
    int player_socks[2];
    char player_name[2][73];
} Game;

void init_game(Game *g, int piles[5], char player_name[2][73], int socks[2]) {
    for (int i = 0; i < 5; i++) {
        g->piles[i] = piles[i];
    }
    g->curr_player = 0;
    for (int i = 0; i < 2; i++) {
        g->player_socks[i] = -1; // Initialize to invalid socket
        strncpy(g->player_name[i], player_name[i], 72);
        g->player_name[i][72] = '\0'; // Ensure null-termination
    }
    
}

int apply_move(Game *g, int pile, int count) {
    return EXIT_SUCCESS;
}

int is_game_over(Game *g) {
    return EXIT_SUCCESS;
}