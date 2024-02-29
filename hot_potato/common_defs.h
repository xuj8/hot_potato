
#ifndef COMMON_DEFS
#define COMMON_DEFS

// #define DEBUG

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <functional>
#include <string>
#include <atomic>
#include <netdb.h>
#include <thread>
#include <vector>

using socketfd_t = int;
using status_t = int;

constexpr static size_t BUFFER_SIZE = 2048;
#endif

