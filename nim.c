#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include "decoder.h"
#include "game.h"

#define QUEUE_SIZE 8

typedef struct {
  int sock;
  char name[73]; //max is 72 + null
  int opened; //bool
  char *buffer;
  int buffer_size;
} Player;

volatile int active = 1;

void handler(int signum) { active = 0; }

void reap(int signum) {
  int pid;
  // repeatedly wait until all zombies are reaped
  do {
    pid = waitpid(-1, NULL, WNOHANG);
  } while (pid > 0);
}

void install_handlers(void) {
  struct sigaction act;
  act.sa_handler = handler;
  sigemptyset(&act.sa_mask);
  sigaddset(&act.sa_mask, SIGINT);
  sigaddset(&act.sa_mask, SIGHUP);
  sigaddset(&act.sa_mask, SIGTERM);
  act.sa_flags = 0;

  sigaction(SIGINT, &act, NULL);
  sigaction(SIGHUP, &act, NULL);
  sigaction(SIGTERM, &act, NULL);

  act.sa_handler = reap;
  act.sa_flags = SA_RESTART;
  sigaction(SIGCHLD, &act, NULL);
}

int connect_inet(char *host, char *service) {
  struct addrinfo hints, *info_list, *info;
  int sock, error;

  // look up remote host
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC; // in practice, this means give us IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM; // indicate we want a streaming socket

  error = getaddrinfo(host, service, &hints, &info_list);
  if (error) {
    fprintf(stderr, "error looking up %s:%s: %s\n", host, service,
            gai_strerror(error));
    return -1;
  }

  for (info = info_list; info != NULL; info = info->ai_next) {
    sock = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (sock < 0)
      continue;

    error = connect(sock, info->ai_addr, info->ai_addrlen);
    if (error) {
      close(sock);
      continue;
    }

    break;
  }
  freeaddrinfo(info_list);

  if (info == NULL) {
    fprintf(stderr, "Unable to connect to %s:%s\n", host, service);
    return -1;
  }

  return sock;
}

int open_listener(char *service, int queue_size) {
  struct addrinfo hint, *info_list, *info;
  int error, sock;

  // initialize hints
  memset(&hint, 0, sizeof(struct addrinfo));
  hint.ai_family = AF_UNSPEC;
  hint.ai_socktype = SOCK_STREAM;
  hint.ai_flags = AI_PASSIVE;

  // obtain information for listening socket
  error = getaddrinfo(NULL, service, &hint, &info_list);
  if (error) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
    return -1;
  }

  // attempt to create socket
  for (info = info_list; info != NULL; info = info->ai_next) {
    sock = socket(info->ai_family, info->ai_socktype, info->ai_protocol);

    // if we could not create the socket, try the next method
    if (sock == -1)
      continue;

    // bind socket to requested port
    error = bind(sock, info->ai_addr, info->ai_addrlen);
    if (error) {
      close(sock);
      continue;
    }

    // enable listening for incoming connection requests
    error = listen(sock, queue_size);
    if (error) {
      close(sock);
      continue;
    }

    // if we got this far, we have opened the socket
    break;
  }

  freeaddrinfo(info_list);

  // info will be NULL if no method succeeded
  if (info == NULL) {
    fprintf(stderr, "Could not bind\n");
    return -1;
  }

  return sock;
}

#define BUFLEN 256

void read_buf(int sock, char *buf, int bufsize) {
  int bytes_read = 0;
  while (bytes_read < bufsize) {
    int n = read(sock, buf + bytes_read, bufsize - bytes_read);
    if (n < 0) {
      perror("read");
      return;
    } else if (n == 0) {
      // connection closed
      return;
    }
    bytes_read += n;
  }
}
\
void send_wait(int sock) {
  char buf[BUFLEN];
  int len = encode_message(buf, BUFLEN, "WAIT");
  if (write(sock, buf, len) < 0) {
    perror("write");
  }
}

int open(Player *p) {
  Message msg;
  int bytes = decode_message(p->buffer, p->buffer_size, &msg);
  
  if (bytes < 0) {
    char *buf;
    int len = encode_fail(buf, sizeof(buf), ERR_INVALID);
    write(p->sock, buf, len);
    return -1;
  }
  if (bytes == 0) {
    return 0;
  }

  // game not open yet
  if (strcmp(msg.type, "OPEN") != 0) {
    char *buf;
    int len = encode_fail(buf, sizeof(buf), ERR_INVALID);
    write(p->sock, buf, len);
    return -1;
  }

  //already opened
  if (p->opened) {
    char *buf;
    int len = encode_fail(buf, sizeof(buf), ERR_ALREADY_OPEN);
    write(p->sock, buf, len);
    return -1;
  }

  strncpy(p->name, msg.fields[0], sizeof(p->name)-1);
  p->name[sizeof(p->name)-1] = '\0';
  p->opened = 1;
  
  memmove(p->buffer, p->buffer + bytes, p->buffer_size - bytes);
  p->buffer_size -= bytes;

  send_wait(p->sock);
  return 1;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    perror("no port number given");
    exit(EXIT_FAILURE);
  }
  struct sockaddr_storage remote_host;
  socklen_t remote_host_len;

  int listener = open_listener(argv[1], QUEUE_SIZE);
  if (listener < 0) {
    exit(EXIT_FAILURE);
  }

  int waiting_sock = -1;

  while (active) {
    remote_host_len = sizeof(remote_host);
    int sock =
        accept(listener, (struct sockaddr *)&remote_host, &remote_host_len);

    if (sock < 0) {
      perror("accept");
      continue;
    }
    if (waiting_sock == -1) {
      waiting_sock = sock;
      printf("Waiting for opponent\n");
    } else {
      if (fork() == 0) {
        Player p1 = {waiting_sock, "", 0, NULL, BUFLEN};
        Player p2 = {sock, "", 0, NULL, BUFLEN};
        while (1) {
          int p1_bytes = read(waiting_sock, p1.buffer, BUFLEN-p1.buffer_size);
          if (p1_bytes <= 0) {
            printf("Player 1 disconnected\n");
            close(p1.sock);
            close(p2.sock);
            exit(EXIT_SUCCESS);
          }
          p1.buffer_size += p1_bytes;

          p1.opened = open(&p1);

          if (p1.opened < 0) {
            close(p1.sock);
            close(p2.sock);
            exit(EXIT_FAILURE);
          }

          int p2_bytes = read(sock, p2.buffer, BUFLEN-p2.buffer_size);
          if (p2_bytes <= 0) {
            printf("Player 2 disconnected\n");
            close(p1.sock);
            close(p2.sock);
            exit(EXIT_SUCCESS);
          }

          p2.opened = open(&p2);

          if (p2.opened < 0) {
            close(p1.sock);
            close(p2.sock);
            exit(EXIT_FAILURE);
          }
        }
        close(listener);
        Message msg;
        char buffer[BUFLEN];
        read_buf(sock, buffer, BUFLEN);
        decode_message(buffer, BUFLEN, &msg);
        if (msg.error_code != 0) {
          char errbuff[BUFLEN];
          encode_fail(errbuff, BUFLEN, msg.error_code);
          send(sock, errbuff, BUFLEN, 0);
          if (msg.error_code == ERR_INVALID || msg.error_code == ERR_LONG_NAME || 
            msg.error_code == ERR_ALREADY_PLAY || msg.error_code == ERR_ALREADY_OPEN || 
            msg.error_code == ERR_NOT_PLAYING) {
            close(sock);
            exit(EXIT_FAILURE);
          }
          char errbuff[BUFLEN];
          encode_fail(errbuff, BUFLEN, msg.error_code);
          send(sock, errbuff, BUFLEN, 0);
        }
        //printf("Received message of type: %s\n", msg.type);
        close(sock);
        exit(EXIT_SUCCESS);
      }
    }

    close(sock);
  }

  fprintf(stderr, "Shutting down\n");
  close(listener);

  return EXIT_SUCCESS;
}
