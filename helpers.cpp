#include "helpers.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

void daemonize()
{
  pid_t pid = fork();
  if (pid < 0) { // got error
    exit(EXIT_FAILURE);
  }
  if (pid > 0) { // close parent
    exit(EXIT_SUCCESS);
  }

  umask(0);

  pid_t sid = setsid();
  if (sid < 0) {
    exit(EXIT_FAILURE);
  }
  if ((chdir("/")) < 0) {
    exit(EXIT_FAILURE);
  }

  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
}
