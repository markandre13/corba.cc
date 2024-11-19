#include <arpa/inet.h>
#include <errno.h>
#include <ev.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include <print>

#include "../src/corba/net/ws/socket.hh"
#include "kaffeeklatsch.hh"

using namespace std;
using namespace kaffeeklatsch;

static void libev_accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
static void libev_read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);

class TcpProtocol;
class TcpConnection;

// libev user data for the listen handler
struct listen_handler_t {
        ev_io watcher;
        TcpProtocol *protocol;
};

// libev user data for the client handler
struct client_handler_t {
        ev_io watcher;
        struct ev_loop *loop;

        // for forwarding data from wslay to corba
        // CORBA::ORB *orb;
        TcpConnection *connection;
};

class TcpProtocol {
        struct ev_loop *loop;

        int listenSocket = -1;
        unique_ptr<listen_handler_t> listenHandler;

    public:
        TcpProtocol(struct ev_loop *loop) : loop(loop) {}
        ~TcpProtocol();
        /** open listen socket for incoming CORBA connections */
        void listen(const char *host, unsigned port);
        /** shutdown listening socket */
        void shutdown();

        shared_ptr<TcpConnection> connect(const char *host, unsigned port);
};

class TcpConnection {
        const char *host;
        unsigned port;
        int fd;

    public:
        TcpConnection(const char *host, unsigned port, int fd = -1) : host(host), port(port), fd(fd) {}

        /**
         * there does not seem an asynchronous variant for connect()
         * connect() does not need for the remote side to accept the connection.
         * if connect() fails, we might want to retry
         * throw CORBA exceptions similar to omniORB
         */
        void up();
};

kaffeeklatsch_spec([] {
    describe("TcpProtocol (new)", [] {
        describe("listen(host, port)", [] {
            it("when the socket is already in use, throws a runtime_error", [] {
                struct ev_loop *loop = EV_DEFAULT;
                auto protocol0 = make_unique<TcpProtocol>(loop);
                protocol0->listen("localhost", 9003);

                expect([&] {
                    auto protocol1 = make_unique<TcpProtocol>(loop);
                    protocol1->listen("localhost", 9003);
                }).to.throw_(runtime_error("TcpProtocol::listen(): localhost:9003: Address already in use"));
            });

            it("shutdown() closes the listen socket", [] {
                struct ev_loop *loop = EV_DEFAULT;

                auto protocol = make_unique<TcpProtocol>(loop);
                protocol->listen("localhost", 9003);
                protocol->shutdown();
                protocol->listen("localhost", 9003);
            });

            it("destructor closes the listen socket", [] {
                struct ev_loop *loop = EV_DEFAULT;

                auto protocol0 = make_unique<TcpProtocol>(loop);
                protocol0->listen("localhost", 9003);

                protocol0.reset();

                auto protocol1 = make_unique<TcpProtocol>(loop);
                protocol1->listen("localhost", 9003);
            });
        });
        describe("connect(host, port)", [] {
            it("when connect()->up() fails, throws a runtime_error", [] {
                struct ev_loop *loop = EV_DEFAULT;
                auto protocol1 = make_unique<TcpProtocol>(loop);
                auto connection = protocol1->connect("localhost", 9004);
                expect([&] {
                    connection->up();
                }).to.throw_(runtime_error("TcpConnection()::up(): localhost:9004: Connection refused"));
            });
            it("can connect", [] {
                struct ev_loop *loop = EV_DEFAULT;
                auto protocol0 = make_unique<TcpProtocol>(loop);
                protocol0->listen("localhost", 9003);

                auto protocol1 = make_unique<TcpProtocol>(loop);
                auto connection = protocol1->connect("localhost", 9003);
                connection->up();

                ev_run(loop, EVRUN_ONCE);

                // orb should now have a connection which is able to receive...
                // only: we DO NOT PUT THE CONNECTION HANDLING INTO CLASS ORB!!! class ConnectionPool!!!
            });
        });
    });
});

TcpProtocol::~TcpProtocol() { shutdown(); }

void TcpProtocol::shutdown() {
    if (listenSocket >= 0) {
        if (listenHandler.get()) {
            ev_io_stop(loop, &listenHandler->watcher);
            listenHandler.reset();
        }

        close(listenSocket);
        listenSocket = -1;
    }
}

void TcpProtocol::listen(const char *host, unsigned port) {
    if (listenSocket >= 0) {
        throw runtime_error(format("TcpProtocol::listen(): already listening"));
    }

    listenSocket = create_listen_socket(host, port);

    if (listenSocket < 0) {
        throw runtime_error(format("TcpProtocol::listen(): {}:{}: {}", host, port, strerror(errno)));
    }

    listenHandler = make_unique<listen_handler_t>();
    listenHandler->protocol = this;
    ev_io_init(&listenHandler->watcher, libev_accept_cb, listenSocket, EV_READ);
    ev_io_start(loop, &listenHandler->watcher);
}

shared_ptr<TcpConnection> TcpProtocol::connect(const char *host, unsigned port) { return make_shared<TcpConnection>(host, port); }

void TcpConnection::up() {
    if (fd >= 0) {
        return;
    }
    fd = connect_to(host, port);
    if (fd < 0) {
        throw runtime_error(format("TcpConnection()::up(): {}:{}: {}", host, port, strerror(errno)));
    }
    println("outgoing: up {}", fd);
}

// called by libev when a client want's to connect
void libev_accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    println("incoming: got client");
    auto handler = reinterpret_cast<listen_handler_t *>(watcher);
    // puts("got client");
    if (EV_ERROR & revents) {
        perror("got invalid event");
        return;
    }

    char ip[INET6_ADDRSTRLEN];
    memset(ip, 0, sizeof(ip));
    uint16_t port = 0;

    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);
    int fd = accept(watcher->fd, (struct sockaddr *)&addr, &addrlen);

    if (set_non_block(fd) == -1 || set_no_delay(fd) == -1) {
        puts("failed to setup");
        close(fd);
        return;
    }

    switch (addr.ss_family) {
        case AF_INET: {
            struct sockaddr_in *sin = (struct sockaddr_in *)&addr;
            inet_ntop(AF_INET, sin, ip, sizeof(ip));
            port = htons(sin->sin_port);
        } break;
        case AF_INET6: {
            struct sockaddr_in6 *sin = (struct sockaddr_in6 *)&addr;
            inet_ntop(AF_INET6, sin, ip, sizeof(ip));
            port = htons(sin->sin6_port);
        } break;
        default:
            println("PEER IS UNKNOWN");
    }
    println("PEER IS [{}]:{}", ip, port);

    auto client_handler = new client_handler_t();
    client_handler->connection = new TcpConnection(ip, port, fd);
    // client_handler->loop = loop;
    // client_handler->orb = handler->protocol->m_orb;
    // // a serve will only send requests when BiDir was negotiated, and then starts with
    // // requestId 1 and increments by 2
    // // (while client starts with requestId 0 and also increments by 2)
    // auto InitialResponderRequestIdBiDirectionalIIOP = 1;
    // client_handler->connection =
    //     new TcpConnection(handler->protocol->m_localAddress, handler->protocol->m_localPort, "frontend", 2, InitialResponderRequestIdBiDirectionalIIOP);
    // handler->protocol->m_orb->connections.push_back(client_handler->connection);
    // client_handler->connection->handler = client_handler;
    ev_io_init(&client_handler->watcher, libev_read_cb, fd, EV_READ);
    ev_io_start(loop, &client_handler->watcher);
}

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
        // hexdump(buffer, nbytes);
        // handler->orb->socketRcvd(handler->connection, buffer, nbytes);
    } else {
        cerr << "recv -> " << nbytes << endl;
        if (nbytes < 0) {
            perror("recv");
        }
        // sleep(60);
        // exit(1);
    }
}
