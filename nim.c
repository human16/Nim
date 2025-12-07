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

void handle_game(int p1_sock, int p2_sock) {
  Player p1 = {p1_sock, "", 1, 0, malloc(BUFLEN), BUFLEN};
  Player p2 = {p2_sock, "", 1, 0, malloc(BUFLEN), BUFLEN};
  while (1) {
    int p1_bytes = read(p1.sock, p1.buffer, p1.buffer_size);
    if (p1_bytes <= 0) {
      printf("Player 1 disconnected\n");
      close(p1.sock);
      close(p2.sock);
      exit(EXIT_SUCCESS);
    }
    p1.buffer_size += p1_bytes;

    p1.opened = openGame(&p1);

    if (p1.opened < 0) {
      close(p1.sock);
      close(p2.sock);
      exit(EXIT_FAILURE);
    }
  }
  while (1) {
    int p2_bytes = read(p2.sock, p2.buffer, p2.buffer_size);
    if (p2_bytes <= 0) {
      printf("Player 2 disconnected\n");
      close(p1.sock);
      close(p2.sock);
      exit(EXIT_SUCCESS);
    }

    p2.opened = openGame(&p2);

    if (p2.opened < 0) {
      close(p1.sock);
      close(p2.sock);
      exit(EXIT_FAILURE);
    }
  }

  // both players opened

  if (strcmp(p1.name, p2.name) == 0) {
    printf("same name\n");
    char buf[BUFLEN];
    int len = encode_fail(buf, BUFLEN, ERR_ALREADY_PLAY);
    write(p1.sock, buf, len);
    write(p2.sock, buf, len);
    close(p1.sock);
    close(p2.sock);
    exit(EXIT_FAILURE);
  }

  // send names to each other
  char buf[BUFLEN];
  int len = encode_message(buf, sizeof(buf), "PLAY", "1", p2.name);
  write(p1.sock, buf, len);
  len = encode_message(buf, sizeof(buf), "PLAY", "1", p1.name);
  write(p2.sock, buf, len);

  playGame(&p1, &p2);

  close(p1.sock);
  close(p2.sock);
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
    int sock = accept(listener, (struct sockaddr *)&remote_host, &remote_host_len);

    if (sock < 0) {
      perror("accept");
      continue;
    }
    if (waiting_sock == -1) {
      waiting_sock = sock;
      printf("Connected from %d\nWaiting for opponent\n", waiting_sock);
    } else {
      if (fork() == 0) {
        printf("Starting game between %d and %d\n", waiting_sock, sock);
        close(listener);
        handle_game(waiting_sock, sock);
        exit(EXIT_SUCCESS);
      }
      waiting_sock = -1;
    }

    close(sock);
  }

  fprintf(stderr, "Shutting down\n");
  close(listener);

  return EXIT_SUCCESS;
}
