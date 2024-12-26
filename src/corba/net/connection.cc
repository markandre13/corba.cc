#include "connection.hh"
#include "protocol.hh"
#include "../stub.hh"

namespace CORBA {

namespace detail {

// TODO: this looks a bit dangerous. the stub is usually up in user space.
//       what's the supposed behaviour when it disappears???
Connection::~Connection() {
    std::println("Connection::~Connection()");
    while(!stubsById.empty()) {
        delete stubsById.begin()->second;
    }
}
Protocol::~Protocol() {}

std::string Connection::str() const { 
    if (protocol) {
        return protocol->local.str() + " -> " + remote.str();
    }
    return "null -> " + remote.str();
}

// auto cmp = [](TcpConnection *a, TcpConnection *b) {
//     if (a->remote.port < b->remote.port) {
//         return true;
//     }
//     if (a->remote.port == b->remote.port) {
//         return a->remote.host < b->remote.host;
//     }
//     return false;
// };

Connection *ConnectionPool::find(const char *host, uint16_t port) const {
    for (auto &c : connections) {
        std::println("ConnectionPool::find(): {}:{} == {}:{} ?", host, port, c->remote.host, c->remote.port);
        if (c->remote.host == host && c->remote.port == port) {
            return c.get();
        }
    }
    std::println("ConnectionPool::find(): {}:{} == empty", host, port);
    return nullptr;
    // auto conn = make_shared<TcpConnection>(nullptr, host, port);
    // auto p = connections.find(conn);
    // if (p == connections.end()) {
    //     return nullptr;
    // }
    // return p->get();
}

void ConnectionPool::print() const {
    for (auto &c : connections) {
        println("{}", c->str());
    }
}

}  // namespace detail

}  // namespace CORBA
