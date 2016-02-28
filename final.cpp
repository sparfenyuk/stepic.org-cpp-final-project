#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <syslog.h>
#include <uv.h>

#include "helpers.h"

uv_loop_t* loop = NULL;

void on_new_connection(uv_stream_t* server, int status)
{
  syslog(LOG_NOTICE, "New connection come");
}


int main(int argc, char * const argv[])
{
  daemonize();
  openlog("stepic.org", 0, LOG_USER);

  cl_initial_values values;
  parse_cl_ordie(argc, argv, values);

  loop = uv_default_loop();
  uv_tcp_t server;
  struct sockaddr_in addr;

  uv_tcp_init(loop, &server);

  uv_ip4_addr(values.ip.c_str(), values.port, &addr);
  uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);

  int r = uv_listen((uv_stream_t*) &server, SOMAXCONN, on_new_connection);
  if (r) {
      syslog(LOG_ERR, "Listen error %s\n", uv_strerror(r));
      closelog();
      return EXIT_FAILURE;
  }

  syslog(LOG_NOTICE, "Working on %s:%d and serving '%s'", values.ip.c_str(), values.port, values.dir.c_str());

  return uv_run(loop, UV_RUN_DEFAULT);
}
