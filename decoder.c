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
int decode_message(char *buf, int len, Message *msg) {
  memset(msg, 0, sizeof(Message));

  if (len < 5)
    return 0;

  if (buf[0] == '0' && buf[1] == '|') {
    msg->version = 0;
  } else {
    msg->error_code = ERR_INVALID;
    return -1;
  }

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

  if (msg->length + 5 > len) {
    return 0;
  }

  memcpy(msg->type, buf + 5, 4);
  msg->type[4] = '\0';

  int fc = expected_fields(msg->type);
  if (fc == -1) {
    msg->error_code = ERR_INVALID;
    return -1;
  }
  msg->field_count = fc;

  char *p = buf + 10;
  int remaining = msg->length - 5;

  for (int i = 0; i < msg->field_count; i++) {
    msg->fields[i] = p;

    char *delim = find_delimiter(p, remaining);
    if (delim == NULL) {
      msg->error_code = ERR_INVALID;
      return -1;
    }

    *delim = '\0';
    remaining -= (delim - p + 1);
    p = delim + 1;
  }

  if (p != buf + 5 + msg->length) {
    msg->error_code = ERR_INVALID;
    return -1;
  }

  if (strcmp(msg->type, TYPE_OPEN) == 0) {
    if (strlen(msg->fields[0]) > MAX_NAME_LEN) {
      msg->error_code = ERR_LONG_NAME;
      return -1;
    }
  }

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

int encode_message(char *buf, int bufsize, char *type, ...) {
  int fc = expected_fields(type);
  if (fc == -1)
    return -1;

  va_list args;
  va_start(args, type);
  char *fields[3];
  for (int i = 0; i < fc; i++) {
    fields[i] = va_arg(args, char *);
  }
  va_end(args);

  int content_len = 5;
  for (int i = 0; i < fc; i++) {
    content_len += strlen(fields[i]) + 1; // plus one for |
  }

  if (5 + content_len > bufsize)
    return -1;

  int pos = sprintf(buf, "0|%02d|%s|", content_len, type);
  for (int i = 0; i < fc; i++) {
    pos += sprintf(buf + pos, "%s|", fields[i]);
  }

  return pos;
}

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
  return encode_message(buf, bufsize, "FAIL", error_string(error_code));
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
