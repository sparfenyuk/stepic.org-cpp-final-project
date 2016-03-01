#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <syslog.h>
#include <uv.h>
#include "http-parser/http_parser.h"

#include "helpers.h"

uv_loop_t* loop = NULL;

static int _id = 0;
struct client_t {
  uv_work_t* work;
  uv_tcp_t* handler;
  http_parser* parser;
  int id;
  client_t()
  : work(NULL)
  , handler(NULL)
  , parser(NULL)
  {
    id = ++_id;
  }
};

void finalize(struct client_t* client)
{
  if (client->handler) {
    uv_close((uv_handle_t*)client->handler, NULL);
    // free(client->handler);
  }

  delete client;
}

client_t* client_from_handler(uv_tcp_t* handler)
{
  return (client_t*)handler->data;
}

int on_http_url(http_parser* parser)
{
  return 0;
}

int on_http_headers_complete(http_parser* parser)
{
  return 0;
}

http_parser_settings *get_http_settings()
{
    static http_parser_settings *parser_settings = NULL;

    if (!parser_settings) {
        parser_settings = (http_parser_settings*)malloc(sizeof(http_parser_settings));
        memset(parser_settings, 0, sizeof(http_parser_settings));
        // parser_settings->on_url              = on_http_url;
        // parser_settings->on_header_field     = on_http_header_field;
        // parser_settings->on_header_value     = on_http_header_value;
        parser_settings->on_headers_complete = on_http_headers_complete;
        // parser_settings->on_body                = on_http_body;
    }

    return parser_settings;
}

bool do_http_parse(struct client_t* client, const char *buf, size_t length)
{
  if (!client->parser) {
    client->parser = (http_parser*)malloc(sizeof(http_parser));
    http_parser_init(client->parser, HTTP_REQUEST);
    client->parser->data = (void*)client;
  }

  size_t nparsed = http_parser_execute(client->parser, get_http_settings(), buf, length);

  if (nparsed != length) {
      syslog(LOG_ERR, "%zu!=%zu: %s", nparsed, length, http_errno_description(HTTP_PARSER_ERRNO(client->parser)));
  }
  return nparsed != length;
}

void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
  *buf = uv_buf_init((char*) malloc(suggested_size), suggested_size);
}

void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
  struct client_t* client = client_from_handler((uv_tcp_t*)stream);
  syslog(LOG_NOTICE, "On read from client's #%d stream", client->id);

  if (nread < 0){
    syslog(LOG_NOTICE, "Nothing to read from client's #%d stream (EOF=%s)",
        client->id, (nread == UV_EOF ? "yes":"no"));
    if (nread == UV_EOF){
    }
  } else if (nread > 0) {
    syslog(LOG_NOTICE, "Request received: \n%s", (const char*)buf->base);

    bool parsed = do_http_parse(client, buf->base, nread);
    if (!parsed) {
      finalize(client);
    }
    free(buf->base);
  }
}

void on_new_client(uv_work_t* req)
{
  struct client_t* client = (client_t*) req->data;
  syslog(LOG_NOTICE, "Get client #%d from queue", client->id);

  uv_stream_t* stream = (uv_stream_t*) client->handler;
  int r = uv_read_start(stream, alloc_buffer, on_read);
  if (r != 0) {
    syslog(LOG_ERR, "Error on reading client's #%d stream: %s",
                    client->id, uv_strerror(r));
    finalize(client);
  }
  else {
    syslog(LOG_NOTICE, "On start reading client's #%d stream", client->id);
  }
}

void on_new_client_end(uv_work_t* req, int status)
{
}

void on_new_connection(uv_stream_t* server, int status)
{
  if (status < 0) {
      syslog(LOG_ERR, "New connection error %s\n", uv_strerror(status));
      return;
  }
  syslog(LOG_NOTICE, "New connection come");

  uv_tcp_t *handler = (uv_tcp_t*) malloc(sizeof(uv_tcp_t));
  uv_tcp_init(loop, handler);

  if (uv_accept(server, (uv_stream_t*) handler) == 0) {
    syslog(LOG_NOTICE, "Connection accepted");

    uv_work_t* req = (uv_work_t*)malloc(sizeof(uv_work_t));

    struct client_t* client = new client_t();
    client->handler = handler;
    handler->data = (void*)client;
    client->work = req;

    req->data = (void*)client;
    if (uv_queue_work(loop, req, on_new_client, NULL) == 0) {
      syslog(LOG_NOTICE, "Client #%d passed to queue", client->id);
    }
  }
  else {
      uv_close((uv_handle_t*) handler, NULL);
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
