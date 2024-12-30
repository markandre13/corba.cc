#include "protocol.hh"
#include "connection.hh"

using namespace std;

namespace CORBA {
namespace detail {

shared_ptr<Connection> WsProtocol::connect(const char *host, unsigned port) { 
    return make_shared<TcpConnection>(this, host, port); 
}

}  // namespace detail
}  // namespace CORBA
