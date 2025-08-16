#include "queue.h"
#include <string.h>

queue *new_queue() {
  queue *newq = (queue *)malloc(sizeof(queue));
  if (!newq)
    return NULL;
  newq->size = 0;
  newq->cap = 16;
  newq->top = 0;
  newq->messages = (char **)malloc(newq->cap * sizeof(char *));
  if (!newq->messages) {
    free(newq);
    return NULL;
  }
  return newq;
}

int realloc_queue(queue *q) {
  char **messages = realloc(q->messages, q->cap * 2 * sizeof(char *));
  if (!messages)
    return -1;
  q->messages = messages;
  q->cap *= 2;
  return 0;
}

void shift_top(queue *q) {
  if (q->top > 0 && q->top + q->size > q->cap) {
    memmove(q->messages, q->messages + q->top, q->size * sizeof(char *));
    q->top = 0;
  }
}

int push_queue(queue *q, const char *data) {
  shift_top(q);
  if (q->size >= q->cap)
    if (realloc_queue(q) == -1)
      return -1;
  size_t len = strlen(data) + 1;
  char *copy = malloc(len);
  if (copy)
    memcpy(copy, data, len);
  else
    return -1;
  q->messages[q->size + q->top] = copy;
  q->size++;
  return 0;
}

void pop_queue(queue *q) {
  shift_top(q);
  if (q->size <= 0)
    return;
  free(q->messages[q->top]);
  q->top++;
  q->size--;
}

const char *get_top_queue(queue *q) {
  if (q->size <= 0)
    return NULL;
  return q->messages[q->top];
}

void free_queue(queue *q) {
  for (int i = q->top; i < q->size + q->top; i++) {
    if (q->messages[i])
      free(q->messages[i]);
  }
  free(q->messages);
  free(q);
}
