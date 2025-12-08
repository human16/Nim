
#define BUFLEN 256

typedef struct {
    int piles[5];
    int curr_player;
    int player_socks[2];
    char player_name[2][73];
} Game;

typedef struct {
  int sock;
  char name[73]; //max is 72 + null
  int p_num;
  int opened; // 0 is no, 1 is yes
  char *buffer;
  int buffer_size;
  int playing;
} Player;

int openGame(Player *p);

void playGame(Player *p1, Player *p2);

void init_game(Game *g);

int apply_move(Game *g, int pile, int count);

int is_game_over(Game *g);