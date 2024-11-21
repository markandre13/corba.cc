#include "socket.hh"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <print>
#include <string>

void ignore_sig_pipe() {
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, 0);
}

std::vector<int> create_listen_socket(const char *hostname, uint16_t port) {
    std::vector<int> result;

    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    auto service = std::to_string(port);

    // println("LISTEN {}:{}", hostname, service);

    struct addrinfo *addrinfo;
    int r = getaddrinfo(hostname, service.c_str(), &hints, &addrinfo);
    if (r != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(r) << std::endl;
        return result;
    }

    // for (struct addrinfo *rp = addrinfo; rp; rp = rp->ai_next) {
    //     auto x = addr2HostAndPort((const struct sockaddr_storage *)rp->ai_addr);
    //     std::println("  LISTEN {} {}", x.host.c_str(), x.port);
    // }

    for (struct addrinfo *rp = addrinfo; rp; rp = rp->ai_next) {
        // std::cerr << "CREATE SOCKET" << std::endl;
        int fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd == -1) {
            std::cerr << "FAILED TO CREATE SOCKET: " << strerror(errno) << std::endl;
            continue;
        }
        int val = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, static_cast<socklen_t>(sizeof(val))) == -1) {
            std::cerr << "FAILED TO REUSE SOCKET: " << strerror(errno) << std::endl;
            close(fd);
            continue;
        }
        if (bind(fd, rp->ai_addr, rp->ai_addrlen) == -1) {
            std::cerr << "FAILED TO BIND SOCKET: " << strerror(errno) << std::endl;
            close(fd);
            continue;
        }
        if (listen(fd, 16) == -1) {
            std::cerr << "FAILED TO LISTEN ON SOCKET: " << strerror(errno) << std::endl;
            close(fd);
            continue;
        }
        result.push_back(fd);
    }
    freeaddrinfo(addrinfo);

    if (result.size() == 0) {
        std::cerr << "FAILED TO CREATE ANY SOCKET" << std::endl;
    } else {
        std::cout << "LISTENING ON " << result.size() << " SOCKETS" << std::endl;
    }

    return result;
}

int connect_to(const char *host, uint16_t port) {
    int fd = -1;
    int r;

    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res;
    auto service = std::to_string(port);
    r = getaddrinfo(host, service.c_str(), &hints, &res);
    if (r != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(r) << std::endl;
        return -1;
    }

    for (struct addrinfo *rp = res; rp; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd == -1) {
            std::cerr << "failed to create socket: " << strerror(errno) << std::endl;
            continue;
        }
        while ((r = connect(fd, rp->ai_addr, rp->ai_addrlen)) == -1 && errno == EINTR);
        if (r == 0) {
            break;
        }
        std::cerr << "failed to connect socket: " << strerror(errno) << std::endl;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

int set_non_block(int fd) {
    int flags, r;
    while ((flags = fcntl(fd, F_GETFL, 0)) == -1 && errno == EINTR);
    if (flags == -1) {
        return -1;
    }
    while ((r = fcntl(fd, F_SETFL, flags | O_NONBLOCK)) == -1 && errno == EINTR);
    if (r == -1) {
        return -1;
    }
    return 0;
}

int set_no_delay(int fd) {
    int val = 1;
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &val, (socklen_t)sizeof(val));
}

HostAndPort addr2HostAndPort(const struct sockaddr_storage *addr) {
    switch (addr->ss_family) {
        case AF_INET: {
            char ip[INET_ADDRSTRLEN];
            memset(ip, 0, sizeof(ip));
            const struct sockaddr_in *saddr = (struct sockaddr_in *)addr;
            inet_ntop(AF_INET, &saddr->sin_addr, ip, sizeof(ip));
            uint16_t port = ntohs(saddr->sin_port);
            return HostAndPort{ip, port};
        } break;
        case AF_INET6: {
            char ip[INET6_ADDRSTRLEN];
            memset(ip, 0, sizeof(ip));
            const struct sockaddr_in6 *saddr = (struct sockaddr_in6 *)addr;
            inet_ntop(AF_INET6, &saddr->sin6_addr, ip, sizeof(ip));
            uint16_t port = ntohs(saddr->sin6_port);  // WRONG
            return HostAndPort{ip, port};
        } break;
    }
    return HostAndPort{"", 0};
}

HostAndPort getLocalName(int fd) {
    struct sockaddr_storage addr;
    socklen_t len = sizeof(addr);
    getsockname(fd, (sockaddr *)&addr, &len);
    return addr2HostAndPort(&addr);
}

HostAndPort getPeerName(int fd) {
    struct sockaddr_storage addr;
    socklen_t len = sizeof(addr);
    getpeername(fd, (sockaddr *)&addr, &len);
    return addr2HostAndPort(&addr);
}