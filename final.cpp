#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <syslog.h>
#include <uv.h>

#include "helpers.h"

uv_loop_t* loop = NULL;

void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
  *buf = uv_buf_init((char*) malloc(suggested_size), suggested_size);
}

void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
  syslog(LOG_NOTICE, "on read");
  if (nread < 0){
    if (nread == UV_EOF){
    }
  } else if (nread > 0) {
    syslog(LOG_NOTICE, "Request received: \n%s", (const char*)buf->base);
  }
  if (buf->base) {
    free(buf->base);
  }
}

void on_new_client(uv_work_t* req)
{
  uv_tcp_t* client = (uv_tcp_t*)req->data;
  int r = uv_read_start((uv_stream_t*)client, alloc_buffer, on_read);
  if (r != 0) {
    syslog(LOG_ERR, "Error on reading client stream: %s.\n",
                    uv_strerror(r));
    uv_close((uv_handle_t*)client, NULL);
  }
  syslog(LOG_NOTICE, "on read start");
}

void on_new_client_end(uv_work_t* req, int status)
{
  syslog(LOG_NOTICE, "on new client end");
  return;
  if (status < 0) {
    ;
  }
  syslog(LOG_NOTICE, "Connection closed");
  uv_close((uv_handle_t*)req->data, NULL);
  free(req);
  req = NULL;
}

void on_new_connection(uv_stream_t* server, int status)
{
  if (status < 0) {
      syslog(LOG_ERR, "New connection error %s\n", uv_strerror(status));
      return;
  }
  syslog(LOG_NOTICE, "New connection come");
  uv_tcp_t *client = (uv_tcp_t*) malloc(sizeof(uv_tcp_t));
  uv_tcp_init(loop, client);
  if (uv_accept(server, (uv_stream_t*) client) == 0) {
    syslog(LOG_NOTICE, "Connection accepted");
    uv_work_t* req = (uv_work_t*)malloc(sizeof(uv_work_t));
    req->data = (void*)client;
    if (uv_queue_work(loop, req, on_new_client, NULL) == 0) {
      syslog(LOG_NOTICE, "Client passed to queue");
    }
  }
  else {
      uv_close((uv_handle_t*) client, NULL);
  }
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
  if (int r = uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0)) {
    syslog(LOG_ERR, "Bind error %s", uv_strerror(r));
    closelog();
    return EXIT_FAILURE;
  }

  if (int r = uv_listen((uv_stream_t*) &server, SOMAXCONN, on_new_connection)) {
      syslog(LOG_ERR, "Listen error %s", uv_strerror(r));
      closelog();
      return EXIT_FAILURE;
  }

  syslog(LOG_NOTICE, "Working on %s:%d and serving '%s'", values.ip.c_str(), values.port, values.dir.c_str());

  return uv_run(loop, UV_RUN_DEFAULT);
}
