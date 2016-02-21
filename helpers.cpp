#include "helpers.h"

#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>

const char* usage_message =
"Options:\n"
"   -h <ip>: IP Adress of web server\n"
"   -p <port>: port of web server\n"
"   -d <dir>: directory to serve";


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

  openlog("stepic.org", 0, LOG_USER);
}

void start_server(int argc, char * const argv[])
{
  opterr = 0;
  const char* ip_address = NULL;
  const char* port = NULL;
  const char* directory = NULL;
  int c;
  while ((c = getopt (argc, argv, "h:p:d:")) != -1)
  {
    switch (c)
      {
      case 'h':
        ip_address = optarg;
        break;
      case 'p':
        port = optarg;
        break;
      case 'd':
        directory = optarg;
        break;
      case '?':
        switch (optopt)
          {
          case 'h':
          case 'p':
          case 'd':
            syslog(LOG_ERR, "Option -%c requires an argument.", optopt);
          default:
            if (isprint (optopt))
              syslog(LOG_ERR,  "Unknown option `-%c'.\n", optopt);
            else
              syslog(LOG_ERR,
                       "Unknown option character `\\x%x'.\n",
                       optopt);
          }
        exit(EXIT_FAILURE);
      default:
        abort ();
      }
  }

  if (!ip_address || !port || !directory) {
    syslog(LOG_ERR, "%s", usage_message);
    exit(EXIT_FAILURE);
  }

  int port_num = atoi(port);
  if (port_num == 0) {
    syslog(LOG_NOTICE, "Using default port 8888");
    port_num = 8888;
  }

  syslog(LOG_NOTICE, "Working on %s:%d and serving '%s'", ip_address, port_num, directory);

}
