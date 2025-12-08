#ifndef DECODER_H
#define DECODER_H

/*
 * decoder.h - NGP Message Encoder/Decoder
 *
 * This module handles the decoding and encoding of NGP messages
 * used for communication between Nim game server and clients
 *
 * Message format: 0|LL|TYPE|field1|field2|...|
 *                | header |     content      |
 *  - Version: single digit (always 0)
 *  - Length: two digits (length of content after haeder)
 *  - Type: 4 ASCII characters
 *  - Fields: 0 to 3 depending on type, seperated by '|'
 *
 *  Memory model: decode_message() modifiers the input buffer in-place,
 *  replaces '|' delimiters with null-terminators. The fields[] pointers
 *  in the message struct pointer directly into the buffer.
 *
 *  DO NOT FREE fields[], and ensure buffer lifetime is valid while using
 *  Message.
 */

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

/*
 * Message struct populated by decode_message()
 *
 * Fields:
 *  version     - protocol version (always 0)
 *  length      - content length
 *  type        - message type, null-terminated
 *  fields      - array of pointers to field strings in buffer
 *  field_count - number of fields
 *  error_code  - set on decode failure, 0 on success
 */
typedef struct {
  int version;
  int length;
  char type[5];
  char *fields[3];
  int field_count;
  int error_code;
} Message;

/*
 * decode_message - decode a NGP message from a buffer
 *
 * @buf:  input buffer containing raw bytes from socket
 * @len:  number of bytes available in buffer
 * @msg:  output sstruct to populate with decoded data
 *
 * returns:
 *    > 0 success, value is number of bytes consumed
 *      0 incomplete message, need to call read() for more data
 *      -1 invalid message, check msg->error_code, send FAIL
 *
 * Example:
 *  Message msg;
 *  int n = decode_message(buf, bytes_received, &msg);
 *  if (n > 0) {
 *    memmove(buf, buf + bytes_received - n);
 *    bytes_received -= n;
 *  } else if (n == 0) {
 *    // need more data
 *  } else {
 *    encode_fail(outbuf, sizeof(outbuf), msg.error_code);
 *  }
 */
int decode_message(char *buf, int bytes_received, Message *msg);

/*
 * encode_message - encode an NGP message into a buffer
 *
 * @buf:     output buffer to write encoded message
 * @bufsize: size of output buffer
 * @type:    message type ("WAIT", "OPEN", "NAME", "PLAY", "MOVE", "OVER",
 * "FAIL")
 * @...:     Field values as char* (refer to spec for field counts per message
 * type)
 *
 * Returns: number of bytes written, or -1 on failure.
 *
 * Example:
 *   char buf[128];
 *   int len = encode_message(buf, sizeof(buf), "PLAY", "1", "1 3 5 7 9");
 *   write(sock, buf, len);
 */
int encode_message(char *buf, int bufsize, char *type, ...);

/*
 * encode_fail - convenience function to encode a FAIL message
 *
 * @buf:        Output buffer
 * @bufsize:    Size of output buffer
 * @error_code: One of the ERR_* constants
 *
 * Returns: number of bytes written, or -1 on failure.
 *
 * Example:
 *   int len = encode_fail(buf, sizeof(buf), ERR_IMPATIENT);
 *   write(sock, buf, len);
 */
int encode_fail(char *buf, int bufsize, int error_code);

#endif
