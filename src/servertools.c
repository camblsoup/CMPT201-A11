#define _POSIX_C_SOURCE 200809L
#include "servertools.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void set_non_blocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    perror("Failed to set non blocking");
    exit(EXIT_FAILURE);
  }
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    perror("Failed to set non blocking");
    exit(EXIT_FAILURE);
  }
}

static void *client_thread(void *args) {
  struct client_args *cargs = (struct client_args *)args;
  set_non_blocking(cargs->client_fd);

  struct pollfd pfd;
  pfd.fd = cargs->client_fd;
  pfd.events = POLLIN;

  char msg_buf[1024];
  size_t total_bytes_read = 0;

  while (atomic_load(&cargs->run)) {
    int ret = poll(&pfd, 1, 500);
    if (ret == -1) {
      perror("Poll error");
      break;
    } else if (ret == 0) {
      continue;
    }

    size_t bytes_read = read(cargs->client_fd, msg_buf + total_bytes_read,
                             1024 - total_bytes_read);
    if (bytes_read <= 0) {
      perror("Client disonnected");
      break;
    }

    total_bytes_read += bytes_read;

    for (ssize_t i = 1; i < total_bytes_read; i++) {
      if (msg_buf[i] != '\n') {
        if (i == 1023) {
          perror("No newline in message");
          exit(EXIT_FAILURE);
        }
        continue;
      }

      uint8_t msg_type = msg_buf[0];
      if (msg_type == 1) {
        printf("Received exit\n");
        atomic_store(&cargs->run, false);
        goto finished;
      }

      size_t msg_len = i - 1;
      char *output = malloc(sizeof(char) * (msg_len + 8));
      output[0] = msg_type;
      memcpy(output + 1, &cargs->client_addr.sin_addr.s_addr, sizeof(uint32_t));
      memcpy(output + 5, &cargs->client_addr.sin_port, sizeof(uint16_t));
      memcpy(output + 7, msg_buf + 1, msg_len);
      output[msg_len + 7] = '\n';
      int numClients = atomic_load(&cargs->s->current_clients);
      pthread_mutex_lock(&cargs->s->client_fds_mutex);
      // printf("Received: %u  %s\n", msg_type, msg_buf + 1);
      for (int i = 0; i < numClients; i++) {
        write(cargs->s->client_fds[i], output, msg_len + 8);
      }
      pthread_mutex_unlock(&cargs->s->client_fds_mutex);

      free(output);
      memmove(msg_buf, msg_buf + i + 1, total_bytes_read - (i + 1));
      total_bytes_read -= i + 1;
      i = 0;
    }
  }
finished:

  atomic_fetch_add(&cargs->s->finished_clients, 1);
  return NULL;
}

int init_server_socket(uint16_t port) {
  int sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == -1) {
    perror("Socket couldn't initialize");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in address;
  memset(&address, 0, sizeof(struct sockaddr_in));
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sfd, (struct sockaddr *)&address, sizeof(struct sockaddr_in)) ==
      -1) {
    perror("Could not bind sfd to addr");
    exit(EXIT_FAILURE);
  }

  if (listen(sfd, 32) == -1) {
    perror("Can't listen to sfd");
    exit(EXIT_FAILURE);
  }

  return sfd;
}

static void *acceptor_thread(void *args) {
  server *s = (server *)args;
  int sfd = init_server_socket(s->port);
  set_non_blocking(sfd);

  pthread_t clients[s->expected_clients];
  struct client_args cargs[s->expected_clients];

  while (atomic_load(&s->run) &&
         atomic_load(&s->finished_clients) < s->expected_clients) {
    if (atomic_load(&s->current_clients) >= s->expected_clients)
      sleep(1);
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int cfd = accept(sfd, (struct sockaddr *)&client_addr, &addr_len);
    if (cfd == -1) {
      continue;
    }
    // run client thread;
    size_t index = atomic_fetch_add(&s->current_clients, 1);
    pthread_mutex_lock(&s->client_fds_mutex);
    s->client_fds[index] = cfd;
    pthread_mutex_unlock(&s->client_fds_mutex);
    atomic_init(&cargs[index].run, true);
    cargs[index].client_fd = cfd;
    cargs[index].s = s;
    cargs[index].client_addr = client_addr;
    pthread_create(&clients[index], NULL, client_thread, &cargs[index]);
  }

  size_t ran_clients = atomic_load(&s->current_clients);
  uint8_t exit_msg[2];
  exit_msg[0] = (uint8_t)1;
  exit_msg[1] = '\n';
  printf("Sending exit\n");
  pthread_mutex_lock(&s->client_fds_mutex);
  for (int i = 0; i < ran_clients; i++) {
    write(cargs[i].client_fd, exit_msg, 2);
    pthread_join(clients[i], NULL);
    shutdown(cargs[i].client_fd, SHUT_RDWR);
    if (close(s->client_fds[i]) == -1) {
      perror("Failed to close server");
      exit(EXIT_FAILURE);
    }
  }
  pthread_mutex_unlock(&s->client_fds_mutex);

  if (close(sfd) == -1) {
    perror("Failed to close server");
    exit(EXIT_FAILURE);
  }
  sem_post(&s->server_finished);
  return NULL;
}

void run_server(server *s) {
  if (!s) {
    perror("Server not defined");
    exit(EXIT_FAILURE);
  }
  pthread_t acceptor;
  pthread_create(&acceptor, NULL, acceptor_thread, s);
  sem_wait(&s->server_finished);
  pthread_join(acceptor, NULL);
}

server init_server(uint16_t port, size_t expected_clients) {
  server s;
  sem_init(&s.server_finished, 0, 0);
  atomic_init(&s.run, true);
  s.port = port;
  s.expected_clients = expected_clients;
  atomic_init(&s.current_clients, 0);
  atomic_init(&s.finished_clients, 0);
  pthread_mutex_init(&s.client_fds_mutex, NULL);
  return s;
}

void destroy_server(server *s) {
  sem_destroy(&s->server_finished);
  pthread_mutex_destroy(&s->client_fds_mutex);
}
