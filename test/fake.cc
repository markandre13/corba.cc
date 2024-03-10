#include "fake.hh"

#include <corba/orb.hh>

using std::println;

CORBA::async<CORBA::detail::Connection *> FakeTcpProtocol::connect(const ::CORBA::ORB *orb, const std::string &hostname, uint16_t port) {
    // println("TcpFakeConnection::connect(\"{}\", {})", hostname, port);
    auto conn = new TcpFakeConnection(this, m_localAddress, m_localPort, hostname, port);
    // printf("TcpFakeConnection::connect() -> %p %s:%u -> %s:%u requestId=%u\n", static_cast<void *>(conn), conn->localAddress().c_str(), conn->localPort(),
    //        conn->remoteAddress().c_str(), conn->remotePort(), conn->requestId);
    connections.push_back(conn);
    co_return conn;
}

CORBA::async<void> FakeTcpProtocol::close() { co_return; }

void TcpFakeConnection::close() {}
void TcpFakeConnection::send(void *buffer, size_t nbyte) {
    // println("TcpFakeConnection::send(...) from {}:{} to {}:{}", m_localAddress, m_localPort, m_remoteAddress, m_remotePort);
    protocol->packets.emplace_back(FakePaket(this, buffer, nbyte));
}

bool transmit(std::vector<FakeTcpProtocol *> &protocols) {
    for (auto src : protocols) {
        if (!src->packets.empty()) {
            // println("found a packet to send");
            auto &packet = src->packets.front();
            for (auto dst : protocols) {
                if (dst->m_localAddress == packet.connection->remoteAddress() && dst->m_localPort == packet.connection->remotePort()) {
                    // println("found destination protocol {}:{}", dst->m_localAddress, dst->m_localPort);
                    TcpFakeConnection *conn = nullptr;
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
                        // println("found no connection on destination, fake listen/accept and create one");
                        dst->m_orb->getConnection(packet.connection->localAddress(), packet.connection->localPort())
                            .then([&](CORBA::detail::Connection *_conn) {
                                conn = dynamic_cast<TcpFakeConnection *>(_conn);
                            });
                        // conn = dynamic_cast<TcpFakeConnection *>();
                    }
                    println("==================== transmit from {}:{} to {}:{} ====================", packet.connection->localAddress(),
                            packet.connection->localPort(), packet.connection->remoteAddress(), packet.connection->remotePort());
                    dst->m_orb->socketRcvd(conn, packet.buffer.data(), packet.buffer.size());
                    src->packets.erase(src->packets.begin());
                    return true;
                }
            }
        }
    }
    return false;
}
