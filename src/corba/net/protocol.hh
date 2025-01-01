#pragma once

#include <ev.h>

#include <memory>

#include "connection.hh"
#include "util/socket.hh"

namespace CORBA {

class ORB;

namespace detail {

class Connection;

class Protocol {
    public:
        ORB *orb = nullptr;
        struct ev_loop *loop;
        HostAndPort local;

        Protocol(struct ev_loop *loop) : loop(loop) {}
        virtual ~Protocol();
        virtual void listen(const char *host, unsigned port) = 0;
        virtual void shutdown() = 0;

        virtual std::shared_ptr<Connection> connectOutgoing(const char *host, unsigned port) = 0;
        virtual std::shared_ptr<Connection> connectIncoming(const char *host, unsigned port, int fd) = 0;
};

}  // namespace detail
}  // namespace CORBA