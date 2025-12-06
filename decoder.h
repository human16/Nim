#ifndef DECODER_H
#define DECODER_H

// error codes from table 1
#define ERR_NONE 0
#define ERR_INVALID 10
#define ERR_LONG_NAME 21
#define ERR_ALREADY_PLAY 22
#define ERR_ALREADY_OPEN 23
#define ERR_NOT_PLAYING 24
#define ERR_IMPATIENT 31
#define ERR_PILE_INDEX 32
#define ERR_QUANTITY 33

typedef struct {
  int version;
  int length;
  char type[5]; // 4 bytes + null-terminated
  char *fields[3];
  int field_count;
  int error_code;
} Message;

int parse_message(char *buf, int bytes_received, Message *msg);
int encode_message(char *buf, int bufsize, char *type, ...);
int encode_fail(char *buf, int bufsize, int error_code);

#endif
