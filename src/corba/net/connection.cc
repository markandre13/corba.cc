#include "connection.hh"
#include "protocol.hh"
#include "../stub.hh"

namespace CORBA {

namespace detail {

// TODO: this looks a bit dangerous. the stub is usually up in user space.
//       what's the supposed behaviour when it disappears???
Connection::~Connection() {
    std::println("Connection::~Connection()");
    // free stubs which are owned by the connection itself...
    // nameServiceStubs.clear();
    if (!stubsById.empty()) {
        throw std::runtime_error(std::format("Connection::~Connection(): still has stubs at {}:{}", __FILE__, __LINE__));
    }
    // while(!stubsById.empty()) {
    //     delete stubsById.begin()->second;
    // }
}
Protocol::~Protocol() {}

std::string Connection::str() const { 
    if (protocol) {
        return protocol->local.str() + " -> " + remote.str();
    }
    return "null -> " + remote.str();
}

// auto cmp = [](Connection *a, Connection *b) {
//     if (a->remote.port < b->remote.port) {
//         return true;
//     }
//     if (a->remote.port == b->remote.port) {
//         return a->remote.host < b->remote.host;
//     }
//     return false;
// };

std::shared_ptr<Connection> ConnectionPool::find(const char *host, uint16_t port) const {
    for (auto &c : connections) {
        std::println("ConnectionPool::find(): {}:{} == {}:{} ?", host, port, c->remote.host, c->remote.port);
        if (c->remote.host == host && c->remote.port == port) {
            return c;
        }
    }
    std::println("ConnectionPool::find(): {}:{} == empty", host, port);
    return std::shared_ptr<Connection>();
}

void ConnectionPool::print() const {
    for (auto &c : connections) {
        println("{}", c->str());
    }
}

}  // namespace detail

}  // namespace CORBA
