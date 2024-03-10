#pragma once

#include <corba/blob.hh>
#include <corba/protocol.hh>
#include <string>
#include <vector>

class TcpFakeConnection;

struct FakePaket {
        FakePaket(TcpFakeConnection *connection, void *buffer, size_t nbytes) : connection(connection), buffer(buffer, nbytes) {}
        TcpFakeConnection *connection;
        CORBA::blob buffer;
};

struct FakeTcpProtocol : public CORBA::detail::Protocol {
        FakeTcpProtocol(CORBA::ORB *orb, const std::string &localAddress, uint16_t localPort) : m_orb(orb), m_localAddress(localAddress), m_localPort(localPort) {}

        CORBA::async<CORBA::detail::Connection*> connect(const CORBA::ORB *orb, const std::string &hostname, uint16_t port);
        CORBA::async<void> close();

        CORBA::ORB *m_orb;
        std::string m_localAddress;
        uint16_t m_localPort;

        std::vector<FakePaket> packets;
        std::vector<TcpFakeConnection*> connections;
};

class TcpFakeConnection : public CORBA::detail::Connection {
        FakeTcpProtocol *protocol;
        std::string m_localAddress;
        uint16_t m_localPort;
        std::string m_remoteAddress;
        uint16_t m_remotePort;

    public:
        TcpFakeConnection(FakeTcpProtocol *protocol, const std::string &localAddress, uint16_t localPort, const std::string &remoteAddress, uint16_t remotePort)
            : protocol(protocol), m_localAddress(localAddress), m_localPort(localPort), m_remoteAddress(remoteAddress), m_remotePort(remotePort) {}

        const std::string& localAddress() const { return m_localAddress; }
        uint16_t localPort() const { return m_localPort; }
        const std::string& remoteAddress() const { return m_remoteAddress; }
        uint16_t remotePort() const { return m_remotePort; }

        void close();
        void send(void *buffer, size_t nbyte);
};

bool transmit(std::vector<FakeTcpProtocol *> &protocols);
