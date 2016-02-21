#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "helpers.h"



int main(int argc, char * const argv[])
{
  daemonize();
  start_server(argc, argv);

  while(true) {
    sleep(5);
  }

  return EXIT_SUCCESS;
}
