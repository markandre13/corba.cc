#pragma once

#include "../protocol.hh"

namespace CORBA {

class ORB;
class Stub;
class GIOPDecoder;

namespace detail {

class TcpProtocol;

struct listen_handler_t {
        ev_io watcher;
        TcpProtocol *protocol;
};

class TcpProtocol : public Protocol {
    private:
        friend class TcpConnection;

        std::vector<std::unique_ptr<listen_handler_t>> listeners;

    public:
        TcpProtocol(struct ev_loop *loop) : Protocol(loop) {}
        ~TcpProtocol();

        /** listen for incoming CORBA connections */
        void listen(const char *host = nullptr, unsigned port = 2809) override;
        /** shutdown listen socket */
        void shutdown() override;

        std::shared_ptr<Connection> connectOutgoing(const char *host, unsigned port) override;
        std::shared_ptr<Connection> connectIncoming(const char *host, unsigned port, int fd) override;
};

}  // namespace detail
}  // namespace CORBA
