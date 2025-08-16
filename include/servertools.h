#pragma once
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>

typedef struct server {
  atomic_bool run;
  sem_t server_finished;
  uint16_t port;
  size_t expected_clients;
  _Atomic size_t current_clients;
  _Atomic size_t finished_clients;
  int client_fds[110];
  pthread_mutex_t client_fds_mutex;
} server;

struct client_args {
  atomic_bool run;
  int client_fd;
  struct sockaddr_in client_addr;
  server *s;
};

server init_server(uint16_t port, size_t expected_clients);
void run_server(server *s);
void destroy_server(server *s);
