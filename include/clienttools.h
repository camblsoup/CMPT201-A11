#pragma once
#include "queue.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>

typedef struct client {
  sem_t listen;
  int server_fd;
  uint16_t port;
  char *ip;
  size_t num_messages;
  queue *messages;
  pthread_mutex_t messages_mutex;
} client;

client init_client(uint16_t port, char *ip, size_t numMessages);
pthread_t *send_messages(client *c);
pthread_t *receive_messages(client *c);
void destroy_client(client *c);
