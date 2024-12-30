#pragma once

#include "../tcp/protocol.hh"

namespace CORBA {

namespace detail {

class WsProtocol : public TcpProtocol {
    public:
        std::shared_ptr<Connection> connect(const char *host, unsigned port) override;
};

}  // namespace detail
}  // namespace CORBA
