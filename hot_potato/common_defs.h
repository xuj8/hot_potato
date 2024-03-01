
#ifndef COMMON_DEFS
#define COMMON_DEFS

#define DEBUG

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

std::string getLocalIP() {
    const char* googleDnsIp = "8.8.8.8";
    uint16_t dnsPort = 53;
    struct sockaddr_in serv;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    // Could not create socket
    if (sock < 0) {
        std::cerr << "Socket error" << std::endl;
        return "Error";
    }

    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr(googleDnsIp);
    serv.sin_port = htons(dnsPort);

    int err = connect(sock, (const sockaddr*)&serv, sizeof(serv));
    if (err < 0) {
        std::cerr << "Error connecting" << std::endl;
        close(sock);
        return "Error";
    }

    sockaddr_in name;
    socklen_t namelen = sizeof(name);
    err = getsockname(sock, (sockaddr*)&name, &namelen);

    char buffer[80];
    const char* p = inet_ntop(AF_INET, &name.sin_addr, buffer, 80);
    if (p != nullptr) {
        std::string localIP(p);
        close(sock);
        return localIP;
    } else {
        // Error handling
        std::cerr << "Could not get local IP address" << std::endl;
        close(sock);
        return "Error";
    }
}
#endif

