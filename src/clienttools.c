#define _GNU_SOURCE
#include "clienttools.h"
#include "queue.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/*
 * `buf` should point to an array that contains random bytes to convert to a
 * hex string.
 *
 * `str` should point to a buffer used to return the hex string of the random
 * bytes. The size of the buffer should be twice the size of the random bytes
 * (since a byte is two characters in hex) plus one for NULL.
 *
 * `size` is the size of the `str` buffer.
 *
 * For example,
 *
 *   uint8_t buf[10];
 *   char str[10 * 2 + 1];
 *   getentropy(buf, 10);
 *   if (convert(buf, sizeof(buf), str, sizeof(str)) != 0) {
 *     exit(EXIT_FAILURE);
 *   }
 */
int convert(uint8_t *buf, ssize_t buf_size, char *str, ssize_t str_size) {
  if (buf == NULL || str == NULL || buf_size <= 0 ||
      str_size < (buf_size * 2 + 1)) {
    return -1;
  }

  for (int i = 0; i < buf_size; i++)
    sprintf(str + i * 2, "%02X", buf[i]);
  str[buf_size * 2] = '\0';

  return 0;
}

void *write_thread(void *args) {
  client *c = (client *)args;
  for (int i = 0; i < c->num_messages; i++) {
    int msg_size = 50;
    uint8_t *msg = malloc(sizeof(uint8_t) * msg_size);
    getentropy(msg, msg_size);
    char *output = malloc(sizeof(char) * ((msg_size * 2) + 2));
    output[0] = (uint8_t)0;
    convert(msg, msg_size, output + 1, ((msg_size * 2) + 2));
    free(msg);
    output[msg_size * 2 + 1] = '\n';
    // printf("\nSending: %s", output);
    write(c->server_fd, output, 102);
    free(output);
  }
  uint8_t exit_msg[2];
  exit_msg[0] = 1;
  exit_msg[1] = '\n';
  printf("\nSending exit\n");
  write(c->server_fd, exit_msg, 2);
  return NULL;
}

pthread_t *send_messages(client *c) {
  pthread_t *send_thread = malloc(sizeof(pthread_t));
  pthread_create(send_thread, NULL, write_thread, c);
  return send_thread;
}

void *read_thread(void *args) {
  client *c = (client *)args;

  struct pollfd pfd;
  pfd.fd = c->server_fd;
  pfd.events = POLLIN;

  char msg_buf[1024];
  size_t total_bytes_read = 0;

  while (true) {

    int ret = poll(&pfd, 1, 500);
    if (ret == -1) {
      perror("Poll error");
      break;
    } else if (ret == 0) {
      continue;
    }

    ssize_t bytes_read = read(c->server_fd, msg_buf + total_bytes_read,
                              sizeof(msg_buf) - total_bytes_read);
    if (bytes_read <= 0) {
      perror("Server disconnected");
      break;
    }

    if ((uint8_t)msg_buf[0] == 1) {
      printf("\nReceived exit\n");
      goto finished;
    }

    total_bytes_read += bytes_read;

    size_t i = 0;
    while (i + 7 < total_bytes_read) {
      size_t newline_pos = i + 7;
      while (newline_pos < total_bytes_read && msg_buf[newline_pos] != '\n') {
        newline_pos++;
      }

      if (newline_pos >= total_bytes_read) {
        break;
      }

      uint8_t msg_type = msg_buf[i];
      if (msg_type == 1) {
        printf("\nReceived exit\n");
        goto finished;
      }

      uint32_t ip;
      memcpy(&ip, &msg_buf[i + 1], sizeof(uint32_t));
      ip = ntohl(ip);

      struct in_addr ip_addr = {.s_addr = htonl(ip)};
      char ip_str[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &ip_addr, ip_str, INET_ADDRSTRLEN);

      uint16_t port;
      memcpy(&port, &msg_buf[i + 5], sizeof(uint16_t));
      port = ntohs(port);

      size_t msg_len = newline_pos - (i + 7);
      char *msg = malloc(msg_len + 1);
      memcpy(msg, &msg_buf[i + 7], msg_len);
      msg[msg_len] = '\0';

      char *output = malloc(msg_len + 10 + 15 + 1 + 1);
      sprintf(output, "%-15s%-10u%s\n", ip_str, port, msg);

      pthread_mutex_lock(&c->messages_mutex);
      push_queue(c->messages, output);
      pthread_mutex_unlock(&c->messages_mutex);

      free(msg);
      free(output);

      i = newline_pos + 1;
      if ((uint8_t)msg_buf[i] == 1)
        goto finished;
    }

    if (i < total_bytes_read) {
      memmove(msg_buf, msg_buf + i, total_bytes_read - i);
      total_bytes_read -= i;
    } else {
      total_bytes_read = 0;
    }
  }

finished:
  sem_post(&c->listen);
  return NULL;
}

pthread_t *receive_messages(client *c) {
  pthread_t *receive_thread = malloc(sizeof(pthread_t));
  pthread_create(receive_thread, NULL, read_thread, c);
  return receive_thread;
}

client init_client(uint16_t port, char *ip, size_t num_messages) {
  client c;
  sem_init(&c.listen, 0, 0);

  c.server_fd = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  inet_pton(AF_INET, ip, &addr.sin_addr);
  connect(c.server_fd, (struct sockaddr *)&addr, sizeof(addr));

  c.port = port;
  c.ip = ip;
  c.num_messages = num_messages;
  c.messages = new_queue();
  pthread_mutex_init(&c.messages_mutex, NULL);
  return c;
}

void destroy_client(client *c) {
  pthread_mutex_destroy(&c->messages_mutex);
  free_queue(c->messages);
}
