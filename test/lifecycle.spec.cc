#include <arpa/inet.h>
#include <errno.h>
#include <ev.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include <print>
#include <set>
#include <thread>

#include "../src/corba/net/ws/socket.hh"
#include "util.hh"

/*

scenarios

* listen
  orb has a public ip and port which can be used on all IORs
* not listening
  each connection has it's own ip/port which the peer knows
* not being able to listen because of nat
  each connection has it's own ip/port
*/

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

class TcpConnection;

class TcpProtocol {
        friend class TcpConnection;
        struct ev_loop *loop;

        std::vector<std::unique_ptr<listen_handler_t>> listeners;

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
    public:
        TcpProtocol *protocol;
        int fd;
        HostAndPort local;
        HostAndPort remote;

        TcpConnection(TcpProtocol *protocol, const char *host, uint16_t port, int fd = -1) : protocol(protocol), fd(fd), remote(HostAndPort{host, port}) {}

        /**
         * there does not seem an asynchronous variant for connect()
         * connect() does not need for the remote side to accept the connection.
         * if connect() fails, we might want to retry
         * throw CORBA exceptions similar to omniORB
         */
        void up();
        void connRefused();

        std::function<void(void *buffer, size_t nbyte)> receiver;

        void send(void *buffer, size_t nbyte);
        void recv(void *buffer, size_t nbyte);

        void print();
};

void TcpConnection::send(void *buffer, size_t nbytes) {
    cout << "SEND" << endl;
    hexdump(buffer, nbytes);
    ssize_t n = ::send(fd, buffer, nbytes, 0);
    if (n != nbytes) {
        perror("send");
    }
}

void TcpConnection::recv(void *buffer, size_t nbytes) {
    cout << "RECV" << endl;
    hexdump(buffer, nbytes);
    if (receiver) {
        receiver(buffer, nbytes);
    }
}

void TcpConnection::connRefused() {
    close(fd);
    fd = -1;
}

void TcpConnection::print() {
    println(" -> {}", remote.str());
    // println("{}:{} -> {}:{}", protocol->localHost, protocol->localPort, localHost, localPort);
}

auto cmp = [](TcpConnection *a, TcpConnection *b) {
    if (a->remote.port < b->remote.port) {
        return true;
    }
    if (a->remote.port == b->remote.port) {
        return a->remote.host < b->remote.host;
    }
    return false;
};

class ConnectionPool {
        // FIXME: this needs to be a map, set
        std::set<TcpConnection *, decltype(cmp)> connections;

    public:
        inline void insert(TcpConnection *conn) { connections.insert(conn); }
        inline void erase(TcpConnection *conn) { connections.erase(conn); }
        inline void clear() { connections.clear(); }
        inline size_t size() { return connections.size(); }
        TcpConnection *find(const char *host, uint16_t port);
        void print();
};

TcpConnection *ConnectionPool::find(const char *host, uint16_t port) {
    TcpConnection conn(nullptr, host, port);
    auto p = connections.find(&conn);
    if (p == connections.end()) {
        return nullptr;
    }
    return *p;
}

void ConnectionPool::print() {
    for (auto c : connections) {
        c->print();
    }
}

ConnectionPool pool;

kaffeeklatsch_spec([] {
    beforeEach([] {
        pool.clear();
    });
    describe("ConnectionPool", [] {
        it("insert and find", [] {
            auto pool = make_unique<ConnectionPool>();
            TcpConnection a80(nullptr, "a", 80);
            TcpConnection b79(nullptr, "b", 79);
            TcpConnection b80(nullptr, "b", 80);
            TcpConnection b81(nullptr, "b", 81);
            TcpConnection c80(nullptr, "c", 80);

            pool->insert(&a80);
            pool->insert(&b79);
            pool->insert(&b80);
            pool->insert(&b81);
            pool->insert(&c80);

            expect(pool->find("a", 80)).to.equal(&a80);
            expect(pool->find("b", 79)).to.equal(&b79);
            expect(pool->find("b", 80)).to.equal(&b80);
            expect(pool->find("b", 81)).to.equal(&b81);
            expect(pool->find("c", 80)).to.equal(&c80);
        });
    });
    fdescribe("TcpProtocol (new)", [] {
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

        // omniORB https://omniorb.sourceforge.io/omni42/omniORB/omniORB004.html
        // connect timeout
        //   * time allowed to connect to peer
        //   * default 10s
        //   * CORBA::TIMEOUT thrown when timeout exceeds
        //     if there are more than one host/ports, the next attempt will use another one
        //   * user can defined custom timeouts per process, thread and object reference
        //   * user defined timeout > 10s
        //     the first call may take this long
        //   * there are idle shutdowns for outgoing and incoming connections
        // call timeout
        //   * time allowed to get a response
        //   * specified in milliseconds
        //   * in case of timeout, CORBA::TRANSIENT exception is thrown an connection is closed
        //   * with non-block connect returns with != 0 and errno = 36

        describe("connect(host, port)", [] {
            // https://stackoverflow.com/questions/17769964/linux-sockets-non-blocking-connect
            // https://cr.yp.to/docs/connect.html
            // Once the system signals the socket as writable, first call getpeername() to see if it connected or not.
            // If that call succeeded, the socket connected and you can start using it. If that call fails with ENOTCONN,
            // the connection failed. To find out why it failed, try to read one byte from the socket read(fd, &ch, 1),
            // which will fail as well but the error you get is the error you would have gotten from connect() if it wasn't
            // non-blocking.

            // TESTS FOR CONNECT
            // =========================
            // CASE: NO LISTENER
            //   ECONNREFUSED Connection refused
            // CASE: LISTENER (connect() is immediatly successful)
            //
            // CASE: NO RESPONSE (PEER HAS DROP RULE), ECONNREFUSED LATER
            // /sbin/iptables -A INPUT -p tcp --dport 9090 -j DROP
            //   EINPROGRESS  Operation now in progress
            // /sbin/iptables -A INPUT -p tcp --dport 9090 -j DROP
            // wait
            // /sbin/iptables -F INPUT
            //   ECONNREFUSED
            // CASE: NO RESPONSE (PEER HAS DROP RULE), accept LATER

            // TESTS FOR PEER DISAPPEARS
            // =========================
            // * PEER CLOSES IT'S SOCKET
            // * DROP RULE ADDED WHILE CONNECTION IS UP

            // TESTS FOR CORBA PACKAGES ARRIVE IN MULTIPLE MESSAGES

            //
            //   ETIMEDOUT    Operation timed out (after 75s)
            it("connect()", [] {
                struct ev_loop *loop = EV_DEFAULT;
                auto client = make_unique<TcpProtocol>(loop);
                auto clientConn = client->connect("192.168.178.105", 9090);
                clientConn->up();
                ev_run(loop);
            });
            it("can connect", [] {
                struct ev_loop *loop = EV_DEFAULT;

                auto server = make_unique<TcpProtocol>(loop);
                server->listen("localhost", 9003);

                auto client = make_unique<TcpProtocol>(loop);
                auto clientConn = client->connect("localhost", 9003);
                clientConn->up();

                expect(pool.size()).to.equal(0);
                ev_run(loop, EVRUN_ONCE);
                expect(pool.size()).to.equal(1);

                auto serverConn = pool.find("::1", getLocalName(clientConn->fd).port);
                expect(serverConn).to.be.not_().equal(nullptr);
                string result;
                serverConn->receiver = [&](void *buffer, size_t nbytes) {
                    result.assign((char *)buffer, nbytes);
                };

                clientConn->send((void *)"hello", 5);

                ev_run(loop, EVRUN_ONCE);
                expect(result).to.equal("hello");
            });
            it("retry until listener is ready");
        });
    });
    describe("TcpConnection", [] {
        describe("up()", [] {
            it("when connect()->up() fails, throws a runtime_error", [] {
                struct ev_loop *loop = EV_DEFAULT;
                auto protocol1 = make_unique<TcpProtocol>(loop);
                auto connection = protocol1->connect("localhost", 9004);
                expect([&] {
                    connection->up();
                }).to.throw_(runtime_error("TcpConnection()::up(): [localhost]:9004: Connection refused"));
            });
        });
    });
});

TcpProtocol::~TcpProtocol() { shutdown(); }

void TcpProtocol::listen(const char *host, unsigned port) {
    auto sockets = create_listen_socket(host, port);
    if (sockets.size() == 0) {
        throw runtime_error(format("TcpProtocol::listen(): {}:{}: {}", host, port, strerror(errno)));
    }

    for (auto socket : sockets) {
        listeners.push_back(make_unique<listen_handler_t>());
        auto &handler = listeners.back();
        handler->protocol = this;
        ev_io_init(&handler->watcher, libev_accept_cb, socket, EV_READ);
        ev_io_start(loop, &handler->watcher);
    }
}

void TcpProtocol::shutdown() {
    for (auto &listener : listeners) {
        ev_io_stop(loop, &listener->watcher);
        close(listener->watcher.fd);
    }
    listeners.clear();
}

shared_ptr<TcpConnection> TcpProtocol::connect(const char *host, unsigned port) { return make_shared<TcpConnection>(this, host, port); }

void TcpConnection::up() {
    println("UP CONFIG IS {}", remote.str());

    if (fd >= 0) {
        return;
    }
    fd = connect_to(remote.host.c_str(), remote.port);
    if (fd < 0) {
        throw runtime_error(format("TcpConnection()::up(): {}: {}", remote.str(), strerror(errno)));
    }

    println("UP LOCAL {}, REMOTE {}", getLocalName(fd).str(), getPeerName(fd).str());

    auto client_handler = new client_handler_t();
    client_handler->connection = this;
    ev_io_init(&client_handler->watcher, libev_read_cb, fd, EV_READ);
    ev_io_start(protocol->loop, &client_handler->watcher);
}

// called by libev when a client want's to connect
void libev_accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    // println("incoming: got client");
    auto handler = reinterpret_cast<listen_handler_t *>(watcher);
    // puts("got client");
    if (EV_ERROR & revents) {
        perror("got invalid event");
        return;
    }

    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);
    int fd = accept(watcher->fd, (struct sockaddr *)&addr, &addrlen);

    if (set_non_block(fd) == -1 || set_no_delay(fd) == -1) {
        puts("failed to setup");
        close(fd);
        return;
    }

    println("ACCEPT LOCAL {}, REMOTE {}", getLocalName(fd).str(), getPeerName(fd).str());

    auto peer = getPeerName(fd);

    auto client_handler = new client_handler_t();
    client_handler->connection = new TcpConnection(handler->protocol, peer.host.c_str(), peer.port, fd);
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

    pool.insert(client_handler->connection);
}

// called by libev when data can be read
void libev_read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    if (EV_ERROR & revents) {
        perror("libev_read_cb(): got invalid event");
        return;
    }
    auto handler = reinterpret_cast<client_handler_t *>(watcher);
    char buffer[8192];
    ssize_t nbytes = ::recv(handler->watcher.fd, buffer, sizeof(buffer), 0);
    if (nbytes < 0) {
        perror("recv");
        if (errno == ECONNREFUSED) {
            handler->connection->connRefused();
            ev_io_stop(loop, watcher);
            delete handler;
        }
        return;
    }
    handler->connection->recv(buffer, nbytes);
}
