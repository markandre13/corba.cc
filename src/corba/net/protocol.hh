#pragma once

#include "util/socket.hh"
#include "connection.hh"

#include <ev.h>
#include <memory>

namespace CORBA {
namespace detail {

class Connection;

class Protocol {
    public:
        struct ev_loop *loop;
        HostAndPort local;

        Protocol(struct ev_loop *loop) : loop(loop) {}
        virtual void listen(const char *host, unsigned port) = 0;
        virtual std::shared_ptr<Connection> connect(const char *host, unsigned port) = 0;
};

}  // namespace detail
}  // namespace CORBA