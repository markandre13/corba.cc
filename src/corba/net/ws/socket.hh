#pragma once

#include <cstdint>
#include <print>
#include <string>
#include <vector>

struct HostAndPort {
        std::string host;
        uint16_t port;
        inline std::string str() { return std::format("[{}]:{}", host, port); }
};

struct SocketHostPort : HostAndPort {
        int fd;
};

std::vector<int> create_listen_socket(const char *hostname, uint16_t port);
int connect_to(const char *host, uint16_t port);

void ignore_sig_pipe();
int set_non_block(int fd);
int set_no_delay(int fd);

HostAndPort addr2HostAndPort(const struct sockaddr_storage *addr);
HostAndPort getLocalName(int fd);
HostAndPort getPeerName(int fd);
