/*
 * Wslay - The WebSocket Library
 *
 * Copyright (c) 2011, 2012 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <string>

void ignore_sig_pipe() {
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, 0);
}

int create_listen_socket(const char *hostname, uint16_t port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
    auto service = std::to_string(port);

    struct addrinfo *addrinfo;
    int r = getaddrinfo(0, service.c_str(), &hints, &addrinfo);
    if (r != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(r) << std::endl;
        return -1;
    }

    int sfd = -1;
    for (struct addrinfo *rp = addrinfo; rp; rp = rp->ai_next) {
        std::cerr << "CREATE SOCKET" << std::endl;
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1) {
            std::cerr << "FAILED TO CREATE SOCKET: " << strerror(errno) << std::endl;
            continue;
        }
        int val = 1;
        if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &val, static_cast<socklen_t>(sizeof(val))) == -1) {
            std::cerr << "FAILED TO REUSE SOCKET: " << strerror(errno) << std::endl;
            close(sfd);
            continue;
        }
        if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            std::cerr << "SUCCEEDED TO BIND SOCKET" << std::endl;
            break;
        }
        std::cerr << "FAILED TO BIND SOCKET:" << strerror(errno) << std::endl;
        close(sfd);
        sfd = -1;
    }
    freeaddrinfo(addrinfo);
    if (sfd == -1) {
        std::cerr << "FAILED TO CREATE ANY SOCKET" << std::endl;
        return -1;
    }

    if (listen(sfd, 16) == -1) {
        std::cerr << "failed to listen on socket " << sfd << ": " << strerror(errno) << std::endl;
        close(sfd);
        return -1;
    }
    std::cerr << "created listen socket " << sfd << std::endl;
    return sfd;
}

int connect_to(const char *host, uint16_t port) {
  struct addrinfo hints;
  int fd = -1;
  int r;
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
      continue;
    }
    while ((r = connect(fd, rp->ai_addr, rp->ai_addrlen)) == -1 &&
           errno == EINTR)
      ;
    if (r == 0) {
      break;
    }
    close(fd);
    fd = -1;
  }
  freeaddrinfo(res);
  return fd;
}

int set_non_block(int fd) {
    int flags, r;
    while ((flags = fcntl(fd, F_GETFL, 0)) == -1 && errno == EINTR)
        ;
    if (flags == -1) {
        return -1;
    }
    while ((r = fcntl(fd, F_SETFL, flags | O_NONBLOCK)) == -1 && errno == EINTR)
        ;
    if (r == -1) {
        return -1;
    }
    return 0;
}

int set_no_delay(int fd) {
    int val = 1;
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &val, (socklen_t)sizeof(val));
}
