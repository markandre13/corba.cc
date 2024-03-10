#pragma once

#include <ev.h>

#include "../protocol.hh"

namespace CORBA {

namespace net {

struct client_handler_t;

class WsConnection : public CORBA::detail::Connection {
        std::string m_localAddress;
        uint16_t m_localPort;
        std::string m_remoteAddress;
        uint16_t m_remotePort;

    public:
        WsConnection(const std::string &localAddress, uint16_t localPort, const std::string &remoteAddress, uint16_t remotePort, uint32_t initialRequestId = 0)
            : Connection(initialRequestId), m_localAddress(localAddress), m_localPort(localPort), m_remoteAddress(remoteAddress), m_remotePort(remotePort) {}

        void addPeer(const std::string_view &hostname, uint16_t port) override {
            // FIXME: hack, this needs to be a list
            m_remoteAddress = hostname;
            m_remotePort = port;
        }

        const std::string &localAddress() const override { return m_localAddress; }
        uint16_t localPort() const override { return m_localPort; }
        const std::string &remoteAddress() const override { return m_remoteAddress; }
        uint16_t remotePort() const override { return m_remotePort; }

        void close() override;
        void send(void *buffer, size_t nbyte) override;

        client_handler_t *handler;
};

struct WsProtocol : public CORBA::detail::Protocol {
        std::string m_localAddress;
        uint16_t m_localPort;
        CORBA::ORB *m_orb;
        struct ev_loop *m_loop;

        void listen(CORBA::ORB *orb, struct ev_loop *loop, const std::string &hostname, uint16_t port);
        void attach(CORBA::ORB *orb, struct ev_loop *loop);

        async<detail::Connection*> connect(const CORBA::ORB *orb, const std::string &hostname, uint16_t port) override;
        CORBA::async<void> close() override;
};

}  // namespace net

}  // namespace CORBA
