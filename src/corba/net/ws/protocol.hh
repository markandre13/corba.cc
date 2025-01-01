#pragma once

#include "../tcp/protocol.hh"

namespace CORBA {

namespace detail {

class WsProtocol : public TcpProtocol {
    public:
        WsProtocol(struct ev_loop *loop) : TcpProtocol(loop) {}
        std::shared_ptr<Connection> connectOutgoing(const char *host, unsigned port) override;
        std::shared_ptr<Connection> connectIncoming(const char *host, unsigned port, int fd) override;
};

}  // namespace detail
}  // namespace CORBA
