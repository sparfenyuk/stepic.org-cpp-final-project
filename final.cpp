#include <cstring>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <syslog.h>
#include <uv.h>
#include "http-parser/http_parser.h"

#include "helpers.h"

uv_loop_t* loop = NULL;
cl_initial_values values;

static int _id = 0;
struct client_t {
  uv_work_t* work;
  uv_tcp_t* handler;
  http_parser* parser;
  std::string url;
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
  syslog(LOG_NOTICE, "Closing connection with client #%d", client->id);
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
int on_http_url(http_parser* parser, const char *at, size_t length)
{
  struct client_t* client = (client_t*) parser->data;
  client->url = std::string(at, length);
  size_t pos = client->url.find_first_of('?');
  if (pos != std::string::npos) {
    client->url = std::string(at, pos);
  }
  // syslog(LOG_NOTICE, "URL: %s", client->url.c_str());
  return 0;
}

int on_http_header_field(http_parser* parser, const char *at, size_t length)
{
  syslog(LOG_NOTICE, "Field: %s", std::string(at, length).c_str());
  return 0;
}

int on_http_header_value(http_parser* parser, const char *at, size_t length)
{
  syslog(LOG_NOTICE, "Value: %s", std::string(at, length).c_str());
  return 0;
}

http_parser_settings *get_http_settings()
{
    static http_parser_settings *parser_settings = NULL;

    if (!parser_settings) {
        parser_settings = (http_parser_settings*)malloc(sizeof(http_parser_settings));
        memset(parser_settings, 0, sizeof(http_parser_settings));
        parser_settings->on_url              = on_http_url;
        // parser_settings->on_header_field     = on_http_header_field;
        // parser_settings->on_header_value     = on_http_header_value;
        // parser_settings->on_headers_complete = on_http_headers_complete;
        // parser_settings->on_body                = on_http_body;
    }

    return parser_settings;
}

void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
  *buf = uv_buf_init((char*) malloc(suggested_size), suggested_size);
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
  return nparsed == length;
}

void on_write_end(uv_write_t* req, int status)
{
  if (status < 0) {
    syslog(LOG_ERR, "Send response error %s\n", uv_strerror(status));
  }
  if(req->bufs) {
    free(req->bufs->base);
  }
  free(req);
  struct client_t* client = (client_t*) req->data;
  if (client) {
    finalize(client);
  }
}

void write_data(uv_stream_t* handler, const char* buffer, size_t len, void* req_data, uv_write_cb cb)
{
  uv_write_t* req = (uv_write_t*) malloc(sizeof(uv_write_t));
  req->data = req_data;
  uv_buf_t bufs = uv_buf_init((char*)malloc(len), len);
  memcpy(bufs.base,buffer,len);
  uv_write(req, handler, &bufs, 1, cb);
}

#define error_404 "HTTP/1.1 404 Not Found\r\n"\
                  "Connection:close\r\n"\
                  "\r\n"
void send_404(struct client_t* client)
{
  syslog(LOG_NOTICE, "File not found %s", client->url.c_str());
  size_t len = strlen(error_404);
  write_data((uv_stream_t*)client->handler, error_404, len, client, on_write_end);
}

void send_file_content(uv_write_t* req, int status);

#define http_ok   "HTTP/1.0 200 OK\r\n"\
                  "Content-Type: text/html\r\n"\
                  "\r\n"
void send_200(struct client_t* client)
{
  size_t len = strlen(http_ok);
  write_data((uv_stream_t*)client->handler, http_ok, len, client, send_file_content);
}

void on_file_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
  struct client_t* client = (client_t*)stream->data;

  if (nread == UV_EOF) {
    syslog(LOG_NOTICE, "Read file %s", ">eof");
    // on_file_read_end(stream, )
  } else if (nread > 0) {
    syslog(LOG_NOTICE, "Read file %d bytes", nread);
    // file_tcp_pipe->nread += nread;
    write_data((uv_stream_t*)client->handler, buf->base, nread, NULL, on_write_end);
    uv_close((uv_handle_t*)stream, NULL);
    finalize(client);
  } else {
    syslog(LOG_NOTICE, "Read file %s", "end");
    // file_tcp_pipe_end(file_tcp_pipe, nread, CALLBACK);
  }
  if (buf->base)
          free(buf->base);
}

void send_file_content(uv_write_t* req, int status)
{
  struct client_t* client = (client_t*) req->data;
  if (client) {
    req->data = NULL;
  }
  on_write_end(req, status);

  std::string path = values.dir + client->url;

  syslog(LOG_NOTICE, "Read file %s", path.c_str());
  uv_pipe_t *file_pipe = (uv_pipe_t*) malloc(sizeof(uv_pipe_t));
  uv_fs_t file_open_req;
  int fd = uv_fs_open(loop, &file_open_req, path.c_str(), O_RDONLY, 0644, NULL);

  if (uv_pipe_init(loop, file_pipe, 0) != 0) {
    finalize(client);
  }
  if (uv_pipe_open(file_pipe, fd) != 0) {
    finalize(client);
  }
  file_pipe->data = client;

  uv_read_start((uv_stream_t *)file_pipe, alloc_buffer, on_file_read);
}

void make_response_get(struct client_t* client)
{
    const std::string& url = client->url;
    std::string path = values.dir + url;
    if (access(path.c_str(), F_OK) == 0) { // file exists
      send_200(client);
    }
    else {
      send_404(client);
    }
}

void make_response(struct client_t* client)
{
  // sys
  switch (client->parser->method) {
    case 1: // GET
      make_response_get(client);
    break;
    default:
    finalize(client);
  }
}

void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
  struct client_t* client = client_from_handler((uv_tcp_t*)stream);
  syslog(LOG_NOTICE, "On read from client's #%d stream", client->id);

  if (nread < 0){
    syslog(LOG_NOTICE, "Nothing to read from client's #%d stream (EOF=%s)",
        client->id, (nread == UV_EOF ? "yes":"no"));
    if (nread == UV_EOF){
      make_response(client);
    }
    else {
      finalize(client);
    }
  } else if (nread > 0) {
    syslog(LOG_NOTICE, "Request received: \n%s", (const char*)buf->base);

    bool parsed = do_http_parse(client, buf->base, nread);
    if (!parsed) {
      finalize(client);
    }
    else {
      syslog(LOG_NOTICE, "Method: %s, URL: %s", http_method_str((enum http_method)client->parser->method), client->url.c_str());
      uv_read_stop(stream);
      make_response(client);
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
  signal(SIGHUP, SIG_IGN);
  
  daemonize();
  openlog("stepic.org", 0, LOG_USER);

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
