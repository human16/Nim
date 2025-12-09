# Nim Game Server - Project 4

## Authors

by240, elp95

## File Structure and Function Documentation

### decoder.h / decoder.c

This module handles encoding and decoding of NGP messages used for communication between the Nim game server and clients.
Look at header file for documentation.

---

### game.h / game.c

This module handles the core game logic and player communication for Nim.

#### Data Structures

**`Game` struct**

```c
typedef struct {
    int piles[5];      // Stone count for each of 5 piles
    int curr_player;   // Current player (1 or 2)
} Game;
```

**`Player` struct**

```c
typedef struct {
    int sock;          // Socket file descriptor
    char name[73];     // Player name (max 72 + null)
    int p_num;         // Player number (1 or 2)
    int opened;        // Whether OPEN message received (0 or 1)
    char *buffer;      // Receive buffer for messages
    int buffer_size;   // Current bytes in buffer
    int playing;       // Whether player is in active game
} Player;
```

#### Key Functions

**`void init_game(Game *g)`**

- Initializes a new game with the default configuration
- Sets piles to 1, 3, 5, 7, 9 stones respectively
- Sets `curr_player` to 1

**`int is_game_over(Game *g)`**

- Checks if the game has ended (all piles empty)
- Returns 1 if game is over, 0 otherwise

**`int do_move(Game *game, int pile, int count)`**

- Executes a move by removing stones from a pile
- Parameters:
  - `game`: pointer to game state
  - `pile`: pile index (0-4)
  - `count`: number of stones to remove
- Returns:
  - `ERR_NONE` (0) on success
  - `ERR_PILE_INDEX` if pile index is out of range
  - `ERR_QUANTITY` if count is invalid (≤0 or >stones in pile)
- Automatically switches current player after successful move

**`void send_msg(int fd, const char *msg, int len)`**

- Helper function to send a complete message through a socket
- Handles partial writes by looping until all bytes are sent
- Parameters:
  - `fd`: socket file descriptor
  - `msg`: message buffer to send
  - `len`: number of bytes to send

**`void send_name(Player *p1, Player *p2)`**

- Sends NAME messages to both players
- Informs each player of their number (1 or 2) and opponent's name
- Called when a match begins

**`void send_wait(int sock)`**

- Sends WAIT message to a client
- Called after receiving OPEN message from client

**`void send_over(Game *g, Player *p1, Player *p2, int winner, int forfeit)`**

- Sends OVER message to both players indicating game end
- Parameters:
  - `g`: current game state
  - `p1`, `p2`: player pointers (can be NULL if disconnected)
  - `winner`: winning player number
  - `forfeit`: 1 if game ended by forfeit, 0 for normal win
- Includes final board state and forfeit flag in message

**`void send_play(Player *p1, Player *p2, Game *g)`**

- Sends PLAY message to the appropriate player whose turn it is
- Includes current player number and board state

**`int openGame(Player *p)`**

- Handles the OPEN handshake for a player
- Validates OPEN message and player name
- Checks for errors:
  - Invalid message format
  - Name too long (>72 characters)
  - Empty name
  - OPEN sent more than once
- Returns:
  - `1` on success (OPEN accepted, WAIT sent)
  - `0` for incomplete message (need more data)
  - `-1` on error (FAIL sent to client)
- Updates player's buffer by removing consumed message bytes

**`void playGame(Player *p1, Player *p2)`**

- Main game loop that manages a complete Nim game between two players
- Flow:
  1. Initializes game state
  2. Sends NAME messages to both players
  3. Sends initial PLAY message
  4. Enters turn loop:
     - Reads MOVE message from current player
     - Validates move
     - Updates game state
     - Sends next PLAY or OVER message
  5. Handles disconnections as forfeits
- Continues until game ends or player disconnects

---

### nim.c

Main server implementation that handles networking and game coordination.

#### Key Functions

**`void handler(int signum)`**

- Signal handler for SIGINT, SIGHUP, SIGTERM
- Sets `active` flag to 0 to initiate graceful shutdown

**`void reap(int signum)`**

- SIGCHLD handler to reap zombie child processes
- Calls `waitpid()` in a loop to reap all terminated children

**`void install_handlers(void)`**

- Installs signal handlers for graceful shutdown and zombie reaping
- Sets up handlers for SIGINT, SIGHUP, SIGTERM, and SIGCHLD

**`int connect_inet(char *host, char *service)`**

- Creates a TCP connection to a remote host
- Parameters:
  - `host`: hostname or IP address
  - `service`: port number as string
- Returns: socket file descriptor, or -1 on failure
- Uses `getaddrinfo()` to resolve address

**`int open_listener(char *service, int queue_size)`**

- Creates a listening TCP socket bound to specified port
- Parameters:
  - `service`: port number as string
  - `queue_size`: maximum length of pending connection queue
- Returns: listener socket file descriptor, or -1 on failure
- Binds to all available interfaces (IPv4 and IPv6)

**`void read_buf(int sock, char *buf, int bufsize)`**

- Helper function to read data from socket until buffer is full
- Handles partial reads by looping

**`void handle_game(int p1_sock, int p2_sock)`**

- Handles a complete game between two players
- Flow:
  1. Creates Player structs for both players
  2. Waits for both players to send OPEN messages
  3. Validates that players have different names
  4. Calls `playGame()` to run the game
  5. Cleans up resources and closes sockets
- Runs in child process created by `fork()`
- Handles errors and disconnections during setup

**`int main(int argc, char **argv)`**

- Main server loop
- Flow:
  1. Validates command line arguments (port number)
  2. Opens listening socket
  3. Enters accept loop:
     - Accepts connection from first player (stores in `waiting_sock`)
     - Accepts connection from second player
     - Forks child process to handle game
     - Parent closes player sockets and resets `waiting_sock`
  4. Continues until interrupted by signal
- Uses fork() to handle multiple concurrent games

---

## Unit Testing

We wrote comprehensive unit tests for the decoder and game modules to ensure correctness before integration.

### test_decoder.c

Tests all aspects of the NGP message encoding/decoding system.

#### Test Categories

**Valid Message Tests (`test_valid_messages`)**

- Tests decoding of all valid message types: WAIT, OPEN, NAME, PLAY, MOVE, OVER, FAIL
- Verifies correct parsing of fields, version, length, and type
- Tests edge cases like empty fields and spaces in names

**Length Validation Tests (`test_length_validation`)**

- Tests messages with incorrect length fields
- Cases: length too short, length exceeds buffer, single-digit length, non-digit characters
- Ensures decoder properly rejects malformed messages

**Incomplete Message Tests (`test_incomplete_messages`)**

- Tests partial messages that need more data from socket
- Cases: version only, partial length, missing delimiter, empty buffer
- Verifies decoder returns 0 for incomplete messages (not errors)

**Invalid Format Tests (`test_invalid_format`)**

- Tests messages with protocol violations
- Cases: unknown message type, wrong version, missing delimiters, wrong field count
- Ensures proper error codes are set

**Edge Case Tests (`test_edge_cases`)**

- Empty names, maximum length names (72 chars), names exceeding limit (73+ chars)
- Multiple messages in buffer
- Special characters in names
- Verifies protocol limits are enforced

**Encoder Tests (`test_encoder`)**

- Tests encoding of all message types
- Verifies correct format and length calculation
- Tests buffer overflow protection
- Round-trip test: encode then decode to verify data integrity

**encode_fail Tests (`test_encode_fail`)**

- Tests FAIL message generation for all error codes
- Verifies correct error strings are included

#### Running Decoder Tests

```bash
make test_decoder
./test_decoder
```

---

### test_game.c

Tests the game logic and player management functions.

#### Test Categories

**init_game() Tests (`test_init_game`)**

- Verifies initial pile configuration (1, 3, 5, 7, 9)
- Checks current player is set to 1
- Tests that re-initialization properly resets state
- Verifies total stone count is 25

**is_game_over() Tests (`test_is_game_over`)**

- Tests game not over at start
- Tests game over when all piles empty
- Tests game not over with stones remaining in various piles
- Tests with large pile values

**do_move() Tests (`test_do_move`)**

- Basic valid moves (remove 2 from pile 1)
- Remove all stones from a pile
- Remove one stone
- Invalid pile indices (negative, out of range)
- Invalid quantities (zero, negative, more than available)
- Remove from empty pile
- Sequential moves until game ends
- Verifies proper error codes returned

**openGame() Tests (`test_openGame`)**

- Valid OPEN message acceptance
- Maximum length name (72 characters)
- Name too long rejection (73+ characters)
- Empty name rejection
- Wrong message type before opening
- Double OPEN (already opened) error
- Incomplete message handling
- Invalid message format rejection
- Buffer consumption after successful OPEN
- Names with spaces and special characters
- WAIT message format verification
- FAIL message generation for various errors

Uses socketpair() to create test connections without actual networking.

**playGame() Tests (`test_playGame`)**

- Verifies playing flag is set
- Tests NAME messages sent to both players with correct player numbers
- Tests initial PLAY message with starting board state
- Verifies player 1 goes first

#### Running Game Tests

```bash
make test_game
./test_game
```

---

## Manual Testing with rawc

```bash
cd clients/src
make
```

### Starting the Server

In one terminal:

```bash
make regular
./nimd 8080
```

The server will print log messages for connections and moves.

### Connecting with rawc

In separate terminals for each player:

```bash
cd clients/src
./rawc localhost 8080
```

### Manual Testing Scenarios

#### Test 1: Normal Game Flow

**Player 1 Terminal:**

```
./rawc localhost 8080
0|11|OPEN|Alice|
```

Expected: Server responds with `Recv  10 [0|05|WAIT|]`

After Player 2 connects, expect:

```
Recv  18 [0|13|NAME|1|Bob|]
Recv  22 [0|17|PLAY|1|1 3 5 7 9|]
```

Now send a move (remove 1 stone from pile 0):

```
0|09|MOVE|0|1|
```

**Player 2 Terminal:**

```
./rawc localhost 8080
0|09|OPEN|Bob|
```

Expected: Server responds with `Recv  10 [0|05|WAIT|]`

Then:

```
Recv  20 [0|13|NAME|2|Alice|]
```

Wait for your turn, then send a move:

```
0|09|MOVE|1|2|
```

Continue alternating moves until all piles are empty. The player who removes the last stone wins.

Final message (example):

```
Recv  23 [0|18|OVER|1|0 0 0 0 0||]
```

This indicates Player 1 won with an empty board.

#### Test 2: Name Too Long (Error Case)

**Player Terminal:**

```
./rawc localhost 8080
0|79|OPEN|AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA|
```

Expected: Server responds with FAIL message:

```
Recv  26 [0|21|FAIL|21 Long Name|]
```

Connection closes.

#### Test 3: Already Open (Error Case)

**Player Terminal:**

```
./rawc localhost 8080
0|11|OPEN|Alice|
```

Wait for WAIT response, then send OPEN again:

```
0|11|OPEN|Alice|
```

Expected: Server responds with FAIL message:

```
Recv  28 [0|23|FAIL|23 Already Open|]
```

Connection closes.

#### Test 4: Invalid Move - Pile Index (Error Case)

During a game, try to remove from non-existent pile:

```
0|09|MOVE|5|1|
```

Expected: Server responds with FAIL:

```
Recv  27 [0|22|FAIL|32 Pile Index|]
```

Game continues, you can retry with a valid move.

#### Test 5: Invalid Move - Quantity (Error Case)

Try to remove more stones than available (pile 0 has 1 stone):

```
0|09|MOVE|0|2|
```

Expected: Server responds with FAIL:

```
Recv  25 [0|20|FAIL|33 Quantity|]
```

Game continues, retry with valid move.

#### Test 6: Player Disconnect (Forfeit)

Start a game with two players. During Player 1's turn, press Ctrl+C in Player 1's terminal.

**Player 2 Terminal** will see:

```
Recv  30 [0|25|OVER|2|0 3 5 7 9|forfeit|]
```

Player 2 wins by forfeit. Board state shows stones remaining.

#### Test 7: Same Name (Error Case)

**Player 1:**

```
0|09|OPEN|Bob|
```

**Player 2:**

```
0|09|OPEN|Bob|
```

Expected: Player 2 receives FAIL:

```
Recv  33 [0|28|FAIL|22 Already Playing|]
```

Connection closes.

#### Test 8: Concurrent Games

Start 4 terminals and create two simultaneous games:

- Terminal 1 & 2: Game between Alice and Bob
- Terminal 3 & 4: Game between Carol and Dave

Both games should proceed independently without interference.

## Testing TLDR

### Automated Testing

- **test_decoder**: 60+ test cases covering message parsing, encoding, validation
- **test_game**: 70+ test cases covering game initialization, move validation, player management

### Manual Testing

- Used rawc client to test protocol implementation
- Verified error handling for all error codes (10, 21-24, 31-33)
- Tested normal game flow from connection to game end
- Tested forfeit scenarios
- Tested concurrent games with fork()
- Verified message formatting and length calculations

### Test Coverage

- All message types: OPEN, WAIT, NAME, PLAY, MOVE, OVER, FAIL
- All error conditions listed in specification
- Edge cases: maximum name length, empty fields, multiple messages in buffer
- Game logic: valid/invalid moves, game end detection, turn switching
- Network handling: disconnections, incomplete messages, malformed data

---

## Protocol Compliance

This implementation fully complies with the NGP specification:

- Version 0 protocol
- All message types supported
- Proper message framing (length + delimiter validation)
- All error codes implemented (10, 21, 22, 23, 24, 31, 32, 33)
- Default board configuration: 1, 3, 5, 7, 9 stones
- Maximum name length: 72 characters
- Forfeit detection on disconnect

---

## Server Capabilities

- Concurrent games using fork()
- Multiple simultaneous matches
- Proper signal handling (SIGINT, SIGCHLD)
- Connection queuing
- Player name validation
- Duplicate name detection
- Complete error handling
- Logging to stdout for debugging
