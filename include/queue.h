#pragma once
#include <stdlib.h>

typedef struct queue {
  size_t size;
  size_t cap;
  size_t top;
  char **messages;
} queue;

queue *new_queue();
int push_queue(queue *q, const char *data);
void pop_queue(queue *q);
const char *get_top_queue(queue *q);
void free_queue(queue *q);
