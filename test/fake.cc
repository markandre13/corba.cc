#include "fake.hh"

#include <corba/orb.hh>

using std::println;

void FakeTcpProtocol::listen(const char *host, unsigned port) {}

std::shared_ptr<CORBA::detail::Connection> FakeTcpProtocol::connectOutgoing(const char *hostname, unsigned port) {
    // println("TcpFakeConnection::create(\"{}\", {})", hostname, port);
    auto conn = std::make_shared<TcpFakeConnection>(this, m_localAddress, m_localPort, hostname, port);
    // printf("TcpFakeConnection::create() -> %p %s:%u -> %s:%u requestId=%u\n", static_cast<void *>(conn), conn->localAddress().c_str(), conn->localPort(),
    //        conn->remoteAddress().c_str(), conn->remotePort(), conn->requestId);
    connections.push_back(conn);
    return conn;
}
std::shared_ptr<CORBA::detail::Connection> FakeTcpProtocol::connectIncoming(const char *hostname, unsigned port, int fd) {
    // println("TcpFakeConnection::create(\"{}\", {})", hostname, port);
    auto conn = std::make_shared<TcpFakeConnection>(this, m_localAddress, m_localPort, hostname, port);
    // printf("TcpFakeConnection::create() -> %p %s:%u -> %s:%u requestId=%u\n", static_cast<void *>(conn), conn->localAddress().c_str(), conn->localPort(),
    //        conn->remoteAddress().c_str(), conn->remotePort(), conn->requestId);
    connections.push_back(conn);
    return conn;
}
void FakeTcpProtocol::shutdown() {}

void TcpFakeConnection::up() {}
void TcpFakeConnection::send(std::unique_ptr<std::vector<char>> &&data) {
    // println("TcpFakeConnection::send(...) from {}:{} to {}:{}", m_localAddress, m_localPort, m_remoteAddress, m_remotePort);
    protocol->packets.emplace_back(FakePaket(this, data->data(), data->size()));
}

bool transmit(std::vector<FakeTcpProtocol *> &protocols) {
    for (auto src : protocols) {
        if (!src->packets.empty()) {
            // println("found a packet to send");
            auto &packet = src->packets.front();
            for (auto dst : protocols) {
                if (dst->m_localAddress == packet.connection->remoteAddress() && dst->m_localPort == packet.connection->remotePort()) {
                    // println("found destination protocol {}:{}", dst->m_localAddress, dst->m_localPort);
                    std::shared_ptr<TcpFakeConnection> conn;
                    for (auto c : dst->connections) {
                        // println("found a connection");
                        if (c->localAddress() == packet.connection->remoteAddress() && c->localPort() == packet.connection->remotePort() &&
                            c->remoteAddress() == packet.connection->localAddress() && c->remotePort() == packet.connection->localPort()) {
                            // println("found a connection to send to");
                            conn = c;
                            break;
                        }
                    }
                    if (conn == nullptr) {
                        conn = std::dynamic_pointer_cast<TcpFakeConnection>(
                            dst->m_orb->getConnection(packet.connection->localAddress(), packet.connection->localPort())
                        );
                    }
                    println("==================== transmit from {}:{} to {}:{} ====================", packet.connection->localAddress(),
                            packet.connection->localPort(), packet.connection->remoteAddress(), packet.connection->remotePort());
                    dst->m_orb->socketRcvd(conn.get(), packet.buffer.data(), packet.buffer.size());
                    src->packets.erase(src->packets.begin());
                    return true;
                }
            }
        }
    }
    return false;
}
