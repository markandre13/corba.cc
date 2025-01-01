#include "protocol.hh"
#include "connection.hh"

using namespace std;

namespace CORBA {
namespace detail {

shared_ptr<Connection> WsProtocol::connectOutgoing(const char *host, unsigned port) { 
    return make_shared<WsConnection>(this, host, port, WsConnectionState::HTTP_CLIENT); 
}
shared_ptr<Connection> WsProtocol::connectIncoming(const char *host, unsigned port, int fd) { 
    auto conn = make_shared<WsConnection>(this, host, port, WsConnectionState::HTTP_SERVER); 
    conn->accept(fd);
    return conn;
}

}  // namespace detail
}  // namespace CORBA
