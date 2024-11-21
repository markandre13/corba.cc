#include "tcp.hh"
#include "ws/socket.hh"

#include "../orb.hh"
#include "../hexdump.hh"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>  // for puts
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include <fstream>
#include <iostream>
#include <print>

using namespace std;

namespace CORBA {

namespace net {

static void libev_accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
static void libev_read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);

// libev user data for the listen handler
struct listen_handler_t {
        ev_io watcher;
        struct ev_loop *loop;
        TcpProtocol *protocol;
};

// libev user data for the client handler
struct client_handler_t {
        ev_io watcher;
        struct ev_loop *loop;

        // for forwarding data from wslay to corba
        CORBA::ORB *orb;
        TcpConnection *connection;
};

/**
 * Add a listen socket for the specified hostname and port to the libev loop
 */
void TcpProtocol::listen(CORBA::ORB *orb, struct ev_loop *loop, const std::string &hostname, uint16_t port) {
    m_localAddress = hostname;
    m_localPort = port;
    m_orb = orb;
    m_loop = loop;

    int fd = create_listen_socket(hostname.c_str(), port)[0];
    if (fd < 0) {
        throw runtime_error(format("TcpProtocol::listen(): {}:{}: {}", hostname, port, strerror(errno)));
    }
    auto accept_watcher = new listen_handler_t;
    accept_watcher->loop = loop;
    accept_watcher->protocol = this;
    ev_io_init(&accept_watcher->watcher, libev_accept_cb, fd, EV_READ);
    ev_io_start(loop, &accept_watcher->watcher);
}

/**
 * Attach the protocol to the libev loop but do not open a server socket.
 */
void TcpProtocol::attach(CORBA::ORB *orb, struct ev_loop *loop) {
    m_orb = orb;
    m_loop = loop;
}

// called by libev when a client want's to connect
void libev_accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    auto handler = reinterpret_cast<listen_handler_t *>(watcher);
    // puts("got client");
    if (EV_ERROR & revents) {
        perror("got invalid event");
        return;
    }

    int fd = accept(watcher->fd, 0, 0);

    if (set_non_block(fd) == -1 || set_no_delay(fd) == -1) {
        puts("failed to setup");
        close(fd);
        return;
    }

    auto client_handler = new client_handler_t();
    client_handler->loop = loop;
    client_handler->orb = handler->protocol->m_orb;
    // a serve will only send requests when BiDir was negotiated, and then starts with
    // requestId 1 and increments by 2
    // (while client starts with requestId 0 and also increments by 2)
    auto InitialResponderRequestIdBiDirectionalIIOP = 1;
    client_handler->connection =
        new TcpConnection(handler->protocol->m_localAddress, handler->protocol->m_localPort, "frontend", 2, InitialResponderRequestIdBiDirectionalIIOP);
    handler->protocol->m_orb->connections.push_back(client_handler->connection);
    client_handler->connection->handler = client_handler;
    ev_io_init(&client_handler->watcher, libev_read_cb, fd, EV_READ);
    ev_io_start(loop, &client_handler->watcher);
}

async<detail::Connection *> TcpProtocol::create(const CORBA::ORB *orb, const std::string &hostname, uint16_t port) {
    // println("TcpProtocol::create(orb, \"{}\", {})", hostname, port);

    int fd = connect_to(hostname.c_str(), port); // TODO: suspend here
    if (set_non_block(fd) == -1 || set_no_delay(fd) == -1) {
        // println("failed to setup");
        ::close(fd);
        co_return nullptr;
    }

    auto client_handler = new client_handler_t();

    client_handler->loop = m_loop;
    client_handler->orb = m_orb;
    // a server will only send requests when BiDir was negotiated, and then starts with
    // requestId 1 and increments by 2
    // (while client starts with requestId 0 and also increments by 2)
    // auto InitialResponderRequestIdBiDirectionalIIOP = 1;

    if (m_localAddress.empty()) {
        // this orb/protocol has no listen port, assume bi-directional iiop and makeup a hostname
        // using a random number also helps avoiding collisions with other orbs on the peer
        uuid_t uuid;
        uuid_string_t uuid_str;
        uuid_generate_random(uuid);
        uuid_unparse_lower(uuid, uuid_str);
        m_localAddress = uuid_str;

        struct sockaddr_in my_addr;
        bzero(&my_addr, sizeof(my_addr));
        socklen_t len = sizeof(my_addr);
        getsockname(fd, (struct sockaddr *)&my_addr, &len);
        m_localPort = ntohs(my_addr.sin_port);
    }
    // println("CONNECT LOCAL SOCKET IS {}:{}", m_localAddress, m_localPort);

    client_handler->connection = new TcpConnection(m_localAddress, m_localPort, hostname, port);
    client_handler->connection->handler = client_handler;
    ev_io_init(&client_handler->watcher, libev_read_cb, fd, EV_READ);
    ev_io_start(m_loop, &client_handler->watcher);

    // println("suspend TcpProtocol::create()");
    // co_await client_handler->sig.suspend();
    // println("resume TcpProtocol::create()");

    co_return client_handler->connection;
}

CORBA::async<void> TcpProtocol::close() { co_return; }
void TcpConnection::close(){};

// called by libev when data can be read
void libev_read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    if (EV_ERROR & revents) {
        perror("libev_read_cb(): got invalid event");
        return;
    }
    auto handler = reinterpret_cast<client_handler_t *>(watcher);
    char buffer[8192];
    ssize_t nbytes = ::recv(handler->watcher.fd, buffer, 8192, 0);
    if (nbytes > 0) {
        // FIXME: what about partial data...?
        cout << "RECEIVED" << endl;
        hexdump(buffer, nbytes);
        handler->orb->socketRcvd(handler->connection, buffer, nbytes);
    } else {
        cerr << "recv -> " << nbytes << endl;
        if (nbytes < 0) {
            perror("recv");
        }
        sleep(60);
        exit(1);
    }
}

// this is called by the ORB to send data
void TcpConnection::send(void *buffer, size_t nbytes) {
    cout << "SEND" << endl;
    hexdump(buffer, nbytes);
    ssize_t n = ::send(handler->watcher.fd, buffer, nbytes, 0);
    if (n != nbytes) {
        perror("send");
    }
}

}  // namespace net

}  // namespace CORBA
