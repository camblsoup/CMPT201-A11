#include "servertools.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
  if (argc != 3) {
    perror("Invalid # of args\n");
    exit(EXIT_FAILURE);
  }

  int portNum = atoi(argv[1]);

  if (portNum > 99999) {
    perror("Invalid Port #\n");
    exit(EXIT_FAILURE);
  }

  size_t numClients = atoi(argv[2]);
  if (numClients > 100) {
    perror("Invalid # of clients (max 100)\n");
    exit(EXIT_FAILURE);
  }

  server s = init_server(portNum, numClients);
  run_server(&s);
  destroy_server(&s);
  printf("Exited properly\n");
}
