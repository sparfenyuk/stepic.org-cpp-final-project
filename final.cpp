#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

const char* usage_message =
"Options:\n"
"   -h <ip>: IP Adress of web server\n"
"   -p <port>: port of web server\n"
"   -d <dir>: directory to serve";

int main(int argc, char * const argv[])
{
  // opterr = 0;
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
            fprintf (stderr, "Option -%c requires an argument.\n", optopt);
          default:
            if (isprint (optopt))
              fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            else
              fprintf (stderr,
                       "Unknown option character `\\x%x'.\n",
                       optopt);
          }
        return 1;
      default:
        abort ();
      }
  }

  if (!ip_address || !port || !directory) {
    printf("%s\n", usage_message);
  }
  else {
    printf("-h=='%s' -p=='%s' -d=='%s'\n", ip_address, port, directory);
  }

  return 0;
}
