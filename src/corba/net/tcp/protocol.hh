#pragma once

#include "../protocol.hh"

namespace CORBA {

class ORB;
class Stub;
class GIOPDecoder;

namespace detail {

// class listen_handler_t;

class TcpProtocol;

struct listen_handler_t {
        ev_io watcher;
        TcpProtocol *protocol;
};

class TcpProtocol : public Protocol {
    public:
        ConnectionPool pool; // REMOVE ME??? Use one in ORB instead???

    private:
        friend class TcpConnection;

        std::vector<std::unique_ptr<listen_handler_t>> listeners;

    public:
        TcpProtocol(struct ev_loop *loop) : Protocol(loop) {}
        ~TcpProtocol();
        /** open listen socket for incoming CORBA connections */
        void listen(const char *host, unsigned port) override;
        /** shutdown listen socket */
        void shutdown();

        std::shared_ptr<Connection> connect(const char *host, unsigned port) override;
};

}  // namespace detail
}  // namespace CORBA
