#include "decoder.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// NGP constants
#define MAX_NAME_LEN 72
#define MAX_MSG_LEN 104
#define MSG_TYPE_LEN 4
#define MAX_FIELDS 3

// message types
#define TYPE_OPEN "OPEN"
#define TYPE_WAIT "WAIT"
#define TYPE_NAME "NAME"
#define TYPE_PLAY "PLAY"
#define TYPE_MOVE "MOVE"
#define TYPE_OVER "OVER"
#define TYPE_FAIL "FAIL"

// as per section 3.2
static int expected_fields(const char *type) {
  if (strcmp(type, TYPE_OPEN) == 0)
    return 1;
  if (strcmp(type, TYPE_WAIT) == 0)
    return 0;
  if (strcmp(type, TYPE_NAME) == 0)
    return 2;
  if (strcmp(type, TYPE_PLAY) == 0)
    return 2;
  if (strcmp(type, TYPE_MOVE) == 0)
    return 2;
  if (strcmp(type, TYPE_OVER) == 0)
    return 3;
  if (strcmp(type, TYPE_FAIL) == 0)
    return 1;

  return -1;
}

// finds first '|' character in buffer
static char *find_delimiter(char *buf, int len) {
  for (int i = 0; i < len; i++) {
    if (buf[i] == '|') {
      return &buf[i];
    }
  }
  return NULL;
}

// expects a pointer to the buffer, the length of the buffer,
// and a pointer to a message struct.
// returns the number of bytes of the message if successful
// -1 if there was an error or 0 if incomplete
int parse_message(char *buf, int len, Message *msg) {
  // init output struct
  memset(msg, 0, sizeof(Message));

  // buffer length needs to be at least 5 bytes
  if (len < 5)
    return 0; // incomplete, need to wait for more bytes

  // check version is 0 and ends with |
  if (buf[0] == '0' && buf[1] == '|') {
    msg->version = 0;
  } else {
    msg->error_code = ERR_INVALID;
    return -1;
  }

  // check message length is a valid integer between 5 and 99
  if (isdigit(buf[2]) && isdigit(buf[3]) && buf[4] == '|') {
    int msg_length = (buf[2] - '0') * 10 + (buf[3] - '0');
    if (msg_length >= 5 && msg_length <= 99) {
      msg->length = msg_length;
    } else {
      msg->error_code = ERR_INVALID;
      return -1;
    }
  } else {
    msg->error_code = ERR_INVALID;
    return -1;
  }

  // total number of bytes (message length + 5 for the header)
  // should not exceed the size of len
  if (msg->length + 5 > len) {
    return 0; // incomplete
  }

  memcpy(msg->type, buf + 5, 4);
  msg->type[4] = '\0';

  int fc = expected_fields(msg->type);
  if (fc == -1) {
    msg->error_code = ERR_INVALID;
    return -1;
  }
  msg->field_count = fc;

  // parse fields by finding each '|' in the buffer and replacing it
  // with a '\0', then adding everything to msg->fields[i]
  char *p = buf + 10;
  int remaining = msg->length - 5; // subtract "TYPE|" (4 bytes + 1 delimiter)

  for (int i = 0; i < msg->field_count; i++) {
    msg->fields[i] = p; // Save field start

    char *delim = find_delimiter(p, remaining);
    if (delim == NULL) {
      msg->error_code = ERR_INVALID;
      return -1;
    }

    *delim = '\0';
    remaining -= (delim - p + 1);
    p = delim + 1;
  }

  // verify at end
  // after parsing all fields, p should point to end of message
  if (p != buf + 5 + msg->length) {
    msg->error_code = ERR_INVALID;
    return -1;
  }

  // verify name length <= MAX_NAME_LEN for OPEN messages
  if (strcmp(msg->type, TYPE_OPEN) == 0) {
    if (strlen(msg->fields[0]) > MAX_NAME_LEN) {
      msg->error_code = ERR_LONG_NAME;
      return -1;
    }
  }

  // verify MOVE messages
  if (strcmp(msg->type, TYPE_MOVE) == 0) {
    for (int i = 0; i < msg->field_count; i++) {
      char *field = msg->fields[i];
      for (int j = 0; field[j] != '\0'; j++) {
        if (!isdigit(field[j])) {
          msg->error_code = ERR_INVALID;
          return -1;
        }
      }
    }
  }

  return 5 + msg->length;
}

int encode_message(char *buf, int bufsize, char *type, ...) { return 999; }

const char *error_string(int error_code) {
  switch (error_code) {
  case ERR_NONE:
    return "No error";
  case ERR_INVALID:
    return "10 Invalid";
  case ERR_LONG_NAME:
    return "21 Long Name";
  case ERR_ALREADY_PLAY:
    return "22 Already Playing";
  case ERR_ALREADY_OPEN:
    return "23 Already Open";
  case ERR_NOT_PLAYING:
    return "24 Not Playing";
  case ERR_IMPATIENT:
    return "31 Impatient";
  case ERR_PILE_INDEX:
    return "32 Pile Index";
  case ERR_QUANTITY:
    return "33 Quantity";
  default:
    return "Unknown error";
  }
}

int encode_fail(char *buf, int bufsize, int error_code) {
  return encode_message(buf, bufsize, TYPE_FAIL, error_string(error_code));
}

void debug_print_message(const Message *msg) {
  printf("Message {\n");
  printf("  version:     %d\n", msg->version);
  printf("  length:      %d\n", msg->length);
  printf("  type:        \"%s\"\n", msg->type);
  printf("  field_count: %d\n", msg->field_count);
  for (int i = 0; i < msg->field_count; i++) {
    printf("  fields[%d]:   \"%s\"\n", i, msg->fields[i]);
  }
  printf("  error_code:  %d\n", msg->error_code);
  printf("}\n");
}
