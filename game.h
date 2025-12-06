typedef struct {
    int piles[5];
    int curr_player;
    int player_socks[2];
    char player_name[2][73];
} Game;

void init_game(Game *g);

int apply_move(Game *g, int pile, int count);

int is_game_over(Game *g);