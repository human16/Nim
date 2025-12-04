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

// to be removed
#define BUFSIZE 256
#define HOSTSIZE 100
#define PORTSIZE 10
void read_data(int sock, struct sockaddr *rem, socklen_t rem_len) {
  char buf[BUFSIZE + 1], host[HOSTSIZE], port[PORTSIZE];
  int bytes, error;

  error =
      getnameinfo(rem, rem_len, host, HOSTSIZE, port, PORTSIZE, NI_NUMERICSERV);
  if (error) {
    fprintf(stderr, "getnameinfo: %s\n", gai_strerror(error));
    strcpy(host, "??");
    strcpy(port, "??");
  }

  printf("Connection from %s:%s\n", host, port);

  while (active && ((bytes = read(sock, buf, BUFSIZE)) > 0)) {
    buf[bytes] = '\0';
    printf("[%s:%s] read %d bytes |%s|\n", host, port, bytes, buf);
  }

  if (bytes == 0) {
    printf("[%s:%s] got EOF\n", host, port);
  } else if (bytes == -1) {
    printf("[%s:%s] terminating: %s\n", host, port, strerror(errno));
  } else {
    printf("[%s:%s] terminating\n", host, port);
  }

  close(sock);
}

#define BUFLEN 256

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

  while (active) {
    remote_host_len = sizeof(remote_host);
    int sock =
        accept(listener, (struct sockaddr *)&remote_host, &remote_host_len);

    if (sock < 0) {
      perror("accept");
      continue;
    }

    if (fork() == 0) {
      close(listener);
      read_data(sock, (struct sockaddr *)&remote_host, remote_host_len);
      exit(EXIT_SUCCESS);
    }

    close(sock);
  }

  fprintf(stderr, "Shutting down\n");
  close(listener);

  return EXIT_SUCCESS;
}
