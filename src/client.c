#include "clienttools.h"
#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
  if (argc < 5) {
    perror("Too few arguments");
    exit(EXIT_FAILURE);
  }
  uint16_t port = (uint16_t)atoi(argv[2]);
  size_t num_messages = atoi(argv[3]);
  client c = init_client(port, argv[1], num_messages);
  pthread_t *send_thread = send_messages(&c);
  pthread_t *receive_thread = receive_messages(&c);
  sem_wait(&c.listen);
  pthread_join(*send_thread, NULL);
  pthread_join(*receive_thread, NULL);
  free(send_thread);
  free(receive_thread);
  close(c.server_fd);

  // printf("\n\n-------- Printing to File --------\n");

  FILE *out_file;
  out_file = fopen(argv[4], "w");

  const char *msg = get_top_queue(c.messages);
  while (msg != NULL) {
    if (!msg)
      break;
    // printf("%s", msg);
    fprintf(out_file, "%s", msg);
    pop_queue(c.messages);
    msg = get_top_queue(c.messages);
  }

  fclose(out_file);
  destroy_client(&c);
  printf("Exited properly\n");
}
