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
