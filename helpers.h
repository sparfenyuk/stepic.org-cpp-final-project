
#pragma once

#include <string>

struct cl_initial_values {
  std::string ip;
  int port;
  std::string dir;
};

void daemonize();
void parse_cl_ordie(int argc, char * const argv[], cl_initial_values& values);
