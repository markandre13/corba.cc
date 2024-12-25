#include "protocol.hh"
#include "connection.hh"
#include "../../orb.hh"
#include "../../exception.hh"

#include <unistd.h>
#include <arpa/inet.h>

using namespace std;

namespace CORBA {
namespace detail {

static void libev_accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);

static string prefix(TcpProtocol *proto) {
    string result;
    if (proto->orb && proto->orb->logname) {
        result += format("ORB({}): ", proto->orb->logname);
    }
    return result;
}

TcpProtocol::~TcpProtocol() { shutdown(); }

void TcpProtocol::listen(const char *host, unsigned port) {
    local.host = host;
    local.port = port;
    auto sockets = create_listen_socket(host, port);
    if (sockets.size() == 0) {
        println("{}TcpProtocol::listen(): {}:{}: {}", prefix(this), host, port, strerror(errno));
        throw CORBA::INITIALIZE(INITIALIZE_TransportError, CORBA::CompletionStatus::YES);
    }
    for (auto socket : sockets) {
        println("{}TcpProtocol::listen(): on {}", prefix(this), getLocalName(socket).str());
        listeners.push_back(make_unique<listen_handler_t>());
        auto &handler = listeners.back();
        handler->protocol = this;
        ev_io_init(&handler->watcher, libev_accept_cb, socket, EV_READ);
        ev_io_start(loop, &handler->watcher);
    }
}

void TcpProtocol::shutdown() {
    for (auto &listener : listeners) {
        println("TcpProtocol::shutdown() {}", getLocalName(listener->watcher.fd).str());
        ev_io_stop(loop, &listener->watcher);
        close(listener->watcher.fd);
    }
    listeners.clear();
}

shared_ptr<Connection> TcpProtocol::connect(const char *host, unsigned port) { return make_shared<TcpConnection>(this, host, port); }

// called by libev when a client want's to connect
void libev_accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    // println("incoming: got client");
    // puts("got client");
    if (EV_ERROR & revents) {
        perror("got invalid event");
        return;
    }

    auto handler = reinterpret_cast<listen_handler_t *>(watcher);

    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);
    int fd = accept(watcher->fd, (struct sockaddr *)&addr, &addrlen);

    set_non_block(fd);
    if (set_non_block(fd) == -1 || set_no_delay(fd) == -1) {
        puts("failed to setup");
        close(fd);
        return;
    }

    auto peer = getPeerName(fd);
    auto connection = make_shared<TcpConnection>(handler->protocol, peer.host.c_str(), peer.port);
    handler->protocol->orb->connections.insert(connection);
    connection->accept(fd);

    println("{}accepted new connection {}", prefix(handler->protocol), connection->str());
}

}  // namespace detail
}  // namespace CORBA
