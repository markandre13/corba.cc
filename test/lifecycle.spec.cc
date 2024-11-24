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
#include <utility>
#include <list>

#include "../src/corba/exception.hh"
#include "../src/corba/net/ws/socket.hh"
#include "util.hh"

#define CORBA_IIOP_PORT 683
#define CORBA_IIOP_SSL_PORT 684
#define CORBA_MANAGEMENT_AGENT_PORT 1050
#define CORBA_LOC_PORT 2809

/*
scenarios

* listen
  orb has a public ip and port which can be used on all IORs
* not listening
  each connection has it's own ip/port which the peer knows
* not being able to listen because of nat
  each connection has it's own ip/port
*/

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

// EHOSTUNREACH No route to host
//   e.g. host down
// ECONNREFUSED Connection refused
//  host is up but no service is listening on that requested port
// ETIMEDOUT    Connection timed out
//  DROP rule, macOS 75s, Linux 134s

#include "kaffeeklatsch.hh"

using namespace std;
using namespace kaffeeklatsch;

static void libev_accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
static void libev_read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
static void libev_write_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);

class TcpProtocol;
class TcpConnection;

class ConnectionPool {
        // FIXME: this needs to be a map, set
        // std::set<shared_ptr<TcpConnection>, decltype(cmp)> connections;
        std::set<shared_ptr<TcpConnection>> connections;

    public:
        inline void insert(shared_ptr<TcpConnection> conn) { connections.insert(conn); }
        inline void erase(shared_ptr<TcpConnection> conn) { connections.erase(conn); }
        inline void clear() { connections.clear(); }
        inline size_t size() { return connections.size(); }
        TcpConnection *find(const char *host, uint16_t port);
        void print();
};

struct listen_handler_t {
        ev_io watcher;
        TcpProtocol *protocol;
};

struct read_handler_t {
        ev_io watcher;
        TcpConnection *connection;
};

struct write_handler_t {
        ev_io watcher;
        TcpConnection *connection;
};

class TcpProtocol {
    public:
        ConnectionPool pool;

    private:
        friend class TcpConnection;
        struct ev_loop *loop;

        std::vector<std::unique_ptr<listen_handler_t>> listeners;

    public:
        TcpProtocol(struct ev_loop *loop) : loop(loop) {}
        ~TcpProtocol();
        /** open listen socket for incoming CORBA connections */
        void listen(const char *host, unsigned port);
        /** shutdown listen socket */
        void shutdown();

        shared_ptr<TcpConnection> connect(const char *host, unsigned port);
};

unique_ptr<vector<char>> str2vec(const char *data) {
    auto vec = make_unique<vector<char>>();
    vec->assign(data, data + strlen(data));
    return vec;
}

enum class ConnectionState {
    /**
     * The connection is not established nor is there a need for it to be established.
     */
    IDLE,
    /**
     * The connection needs to be establish but attempt to connect to the remote host has been made yet.
     */
    PENDING,
    /**
     * An attempt to connect to the remote host is in progress.
     */
    INPROGRESS,
    /**
     * A connection to the remote host has been established.
     */
    ESTABLISHED
};

class TcpConnection {
        int fd = -1;
        std::unique_ptr<read_handler_t> readHandler;
        std::unique_ptr<write_handler_t> writeHandler;

        list<unique_ptr<vector<char>>> sendBuffer;
        ssize_t bytesSend = 0;

    public:
        ConnectionState state = ConnectionState::IDLE;

        TcpProtocol *protocol;
        HostAndPort local;
        HostAndPort remote;

        TcpConnection(TcpProtocol *protocol, const char *host, uint16_t port) : protocol(protocol), remote(HostAndPort{host, port}) {}
        ~TcpConnection();

        void up();
        void accept(int fd);
        void canWrite();
        void canRead();

        std::function<void(void *buffer, size_t nbyte)> receiver;

        void send(unique_ptr<vector<char>> &&);
        void recv(void *buffer, size_t nbyte);

        void print();
        inline int getFD() { return this->fd; }

    private:
        void startReadHandler();
        void stopReadHandler();
        void startWriteHandler();
        void stopWriteHandler();
};

kaffeeklatsch_spec([] {
    fdescribe("networking", [] {
        describe("ConnectionPool", [] {
            it("insert and find", [] {
                auto pool = make_unique<ConnectionPool>();
                auto a80 = make_shared<TcpConnection>(nullptr, "a", 80);
                auto b79 = make_shared<TcpConnection>(nullptr, "b", 79);
                auto b80 = make_shared<TcpConnection>(nullptr, "b", 80);
                auto b81 = make_shared<TcpConnection>(nullptr, "b", 81);
                auto c80 = make_shared<TcpConnection>(nullptr, "c", 80);

                pool->insert(a80);
                pool->insert(b79);
                pool->insert(b80);
                pool->insert(b81);
                pool->insert(c80);

                expect(pool->find("a", 80)).to.equal(a80.get());
                expect(pool->find("b", 79)).to.equal(b79.get());
                expect(pool->find("b", 80)).to.equal(b80.get());
                expect(pool->find("b", 81)).to.equal(b81.get());
                expect(pool->find("c", 80)).to.equal(c80.get());

                // pool->erase(&a80);
                // pool->erase(&b79);
                // pool->erase(&b80);
                // pool->erase(&b81);
                // pool->erase(&c80);
            });
        });
        describe("TcpProtocol::listen(host, port)", [] {
            it("when the socket is already in use, it throws a CORBA::INITIALIZE exception", [] {
                struct ev_loop *loop = EV_DEFAULT;
                auto protocol0 = make_unique<TcpProtocol>(loop);
                protocol0->listen("localhost", 9003);

                expect([&] {
                    auto protocol1 = make_unique<TcpProtocol>(loop);
                    protocol1->listen("localhost", 9003);
                }).to.throw_(CORBA::INITIALIZE(INITIALIZE_TransportError, CORBA::CompletionStatus::YES));
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

        describe("TcpProtocol::connect(host, port)", [] {
            it("creates a connection", [] {
                struct ev_loop *loop = EV_DEFAULT;
                auto client = make_unique<TcpProtocol>(loop);
                auto clientConn = client->connect("mark13.org", CORBA_IIOP_PORT);

                // THEN the connection is for the given remote host and port
                expect(clientConn->remote.host).to.equal("mark13.org");
                expect(clientConn->remote.port).to.equal(CORBA_IIOP_PORT);
                // THEN the connection is idle
                expect(clientConn->state).to.equal(ConnectionState::IDLE);
            });
        });

        describe("TcpConnection::send()", [] {
            it("no listener, no data to be transmitted: IDLE -> INPROGRESS -> IDLE", [] {
                // expect(system("/sbin/iptables -F INPUT")).to.equal(0);
                // expect(system("/sbin/iptables -A INPUT -p tcp --dport 9090 -j DROP")).to.equal(0);
                struct ev_loop *loop = EV_DEFAULT;
                auto client = make_unique<TcpProtocol>(loop);
                auto clientConn = client->connect("127.0.0.1", 9090);

                clientConn->up();
                expect(clientConn->state).to.equal(ConnectionState::INPROGRESS);

                ev_run(loop, 0);
                expect(clientConn->state).to.equal(ConnectionState::IDLE);
                // expect(system("/sbin/iptables -v -L INPUT")).to.equal(0);
            });

            it("listener, no data transmitted: IDLE -> INPROGRESS -> ESTABLISHED", [] {
                struct ev_loop *loop = EV_DEFAULT;

                // GIVEN a listening server
                auto server = make_unique<TcpProtocol>(loop);
                server->listen("127.0.0.1", 9003);

                // GIVEN a client
                auto client = make_unique<TcpProtocol>(loop);
                auto clientConn = client->connect("127.0.0.1", 9003);

                // WHEN the client is asked to connect
                clientConn->up();

                // THEN the connection is in state INPROGRESS
                expect(clientConn->state).to.equal(ConnectionState::INPROGRESS);

                // WHEN the server accepts the connection
                ev_run(loop, EVRUN_ONCE);

                // THEN both server and client connections are in state ESTABLISHED
                expect(clientConn->state).to.equal(ConnectionState::ESTABLISHED);

                auto serverConn = server->pool.find("127.0.0.1", getLocalName(clientConn->getFD()).port);
                expect(serverConn).to.be.not_().equal(nullptr);
                expect(serverConn->state).to.equal(ConnectionState::ESTABLISHED);
            });

            it("when connections are established, data can be transmitted in both directions", [] {
                struct ev_loop *loop = EV_DEFAULT;

                auto server = make_unique<TcpProtocol>(loop);
                server->listen("localhost", 9003);

                auto client = make_unique<TcpProtocol>(loop);
                auto clientConn = client->connect("localhost", 9003);
                clientConn->up();

                ev_run(loop, EVRUN_ONCE);
                expect(clientConn->state).to.equal(ConnectionState::ESTABLISHED);

                auto serverConn = server->pool.find("::1", getLocalName(clientConn->getFD()).port);
                expect(serverConn).to.be.not_().equal(nullptr);
                expect(serverConn->state).to.equal(ConnectionState::ESTABLISHED);

                string serverReceived;
                serverConn->receiver = [&](void *buffer, size_t nbytes) {
                    serverReceived.assign((char *)buffer, nbytes);
                };
                clientConn->send(str2vec("hello"));

                expect(clientConn->state).to.equal(ConnectionState::ESTABLISHED);

                ev_run(loop, EVRUN_ONCE);
                expect(serverReceived).to.equal("hello");

                string clientReceived;
                clientConn->receiver = [&](void *buffer, size_t nbytes) {
                    clientReceived.assign((char *)buffer, nbytes);
                };

                serverConn->send(str2vec("hello"));
                ev_run(loop, EVRUN_ONCE);
                expect(clientReceived).to.equal("hello");
            });
            fit("buffer outgoing data", [] {
                struct ev_loop *loop = EV_DEFAULT;

                auto server = create_listen_socket("127.0.0.1", 9003)[0];

                // auto server = make_unique<TcpProtocol>(loop);
                // server->listen("127.0.0.1", 9003);

                auto client = make_unique<TcpProtocol>(loop);
                auto clientConn = client->connect("127.0.0.1", 9003);
                clientConn->up();

                int serverConn = accept(server, nullptr, nullptr);
                set_non_block(serverConn);

                ev_run(loop, EVRUN_ONCE);

                // auto serverConn = server->pool.find("127.0.0.1", getLocalName(clientConn->getFD()).port);
                // expect(serverConn).to.be.not_().equal(nullptr);
                // expect(serverConn->state).to.equal(ConnectionState::ESTABLISHED);

                int serverRecvBufSize = 0;
                socklen_t m = sizeof(serverRecvBufSize);
                // setsockopt(serverConn, SOL_SOCKET, SO_RCVBUF, (void *)&serverRecvBufSize, m);
                getsockopt(serverConn, SOL_SOCKET, SO_RCVBUF, (void *)&serverRecvBufSize, &m);

                println("===== send 26 packets");

                for(int i=0; i<26; ++i) {
                    clientConn->send(make_unique<vector<char>>(serverRecvBufSize, 'a' + i));
                    ev_run(loop, EVRUN_ONCE | EVRUN_NOWAIT);
                }

                println("===== receive 26 packets");

                size_t receivedTotal = 0;
                while(true) {
                    char in[131072];
                    ssize_t n;
                    n = recv(serverConn, in, sizeof(in), 0);
                    if (n >= 0) {
                        receivedTotal += n;
                        println("server: got {} (total {}, want {})", n, receivedTotal, (26 * serverRecvBufSize));
                        if (receivedTotal == 26 * serverRecvBufSize) {
                            break;
                        }
                    } else {
                        if (errno == EAGAIN) {
                            ev_run(loop, EVRUN_ONCE | EVRUN_NOWAIT);
                            sleep(1);
                        } else {
                            println("server: got error: {} ({})", strerror(errno), errno);
                        }
                    }
                    ev_run(loop, EVRUN_ONCE | EVRUN_NOWAIT);
                    // hexdump(in, n);
                }

            });

            it("retry until listener is ready");

            // ConnectionPool needs a map, TcpProtocol needs to own the connections
            // connection can have different remote host:port names
            // * the a hostname used during connect
            // * the remote ip we see when the connection is established
            // * the remote name we've got during IIOP

            // cache multiple sends ()

            // discover/handle peer shutdown

            // throw CORBA::TRANSIENT and CORBA::COMM_FAILURE as needed
            // throw them on multiple waiting requests as needed
            // also have a look at the minor codes omniORB uses
            // TRANSIENT: failed to reach the object, ie. connect failure
            //    1 Request discarded because of resource exhaustion in POA, or because POA is in discarding state.
            //    2 No usable profile in IOR
            //    3 Request Canceled
            //    4 POA destroyed
            // COMM_FAILURE: failed while communicating with object
        });
    });
});

TcpProtocol::~TcpProtocol() { shutdown(); }

void TcpProtocol::listen(const char *host, unsigned port) {
    auto sockets = create_listen_socket(host, port);
    if (sockets.size() == 0) {
        println("TcpProtocol::listen(): {}:{}: {}", host, port, strerror(errno));
        throw CORBA::INITIALIZE(INITIALIZE_TransportError, CORBA::CompletionStatus::YES);
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
    // listeners.clear();
}

shared_ptr<TcpConnection> TcpProtocol::connect(const char *host, unsigned port) { return make_shared<TcpConnection>(this, host, port); }

void TcpConnection::send(unique_ptr<vector<char>> &&buffer) {
    sendBuffer.push_back(move(buffer));
    startWriteHandler();
}

void TcpConnection::recv(void *buffer, size_t nbytes) {
    // cout << "RECV" << endl;
    // hexdump(buffer, nbytes);
    if (receiver) {
        receiver(buffer, nbytes);
    }
}

void TcpConnection::accept(int client) {
    if (readHandler) {
        println("TcpConnection::accept(int fd): we already have a handler");
        stopReadHandler();
    }
    if (fd != -1) {
        println("TcpConnection::accept(int fd): fd is already set");
        ::close(fd);
    }

    fd = client;
    readHandler = make_unique<read_handler_t>();
    readHandler->connection = this;

    ev_io_init(&readHandler->watcher, libev_read_cb, fd, EV_READ);
    ev_io_start(protocol->loop, &readHandler->watcher);

    state = ConnectionState::ESTABLISHED;
}

TcpConnection::~TcpConnection() {
    stopWriteHandler();
    stopReadHandler();
    if (fd != -1) {
        ::close(fd);
    }
}

void TcpConnection::canWrite() {
    stopWriteHandler();

    if (state == ConnectionState::INPROGRESS) {
        state = ConnectionState::ESTABLISHED;
    }

    while (!sendBuffer.empty()) {
        auto data = sendBuffer.front()->data();
        auto nbytes = sendBuffer.front()->size();

        ssize_t n = ::send(fd, data + bytesSend, nbytes - bytesSend, 0);

        if (n >= 0) {
            println("canWrite(): sendbuffer size {}: send {} bytes at {} of out {}", sendBuffer.size(), n, bytesSend, nbytes - bytesSend);
        } else {
            if (errno != EAGAIN) {
                println("canWrite(): sendbuffer size {}: error: {} ({})", sendBuffer.size(), strerror(errno), errno);
            } else {
                println("canWrite(): sendbuffer size {}: wait", sendBuffer.size());
            }
            break;
        }

        if (n < 0) {
            println("  handle error (implementation missing)");
            break;
        }
        if (n != nbytes - bytesSend) {
            // FOR NOW TO REPRODUCE THIS, THE TEST NEEDS TO BE RUN MULTIPLE TIMES
            println("************************************************** incomplete send");
            bytesSend += n;
            break;
        } else {
            sendBuffer.pop_front();
            bytesSend = 0;
        }
    }

    if (!sendBuffer.empty()) {
        startWriteHandler();
    }
    // auto data = buffer->data();
    // auto nbytes = buffer->size();
    // cout << "SEND" << endl;
    // // hexdump(data, nbytes);
    // ssize_t n = ::send(fd, data, nbytes, 0);
    // println("send {} bytes of out {} ({})", n, nbytes, strerror(errno));
    // if (n != nbytes) {
    // } else {
    //     if (state == ConnectionState::INPROGRESS) {
    //         state = ConnectionState::ESTABLISHED;
    //     }
    // }
}

void TcpConnection::canRead() {
    char buffer[8192];
    ssize_t nbytes = ::recv(fd, buffer, sizeof(buffer), 0);
    if (nbytes < 0) {
        println("TcpConnection::canRead(): {} ({})", strerror(errno), errno);
        stopReadHandler();
        ::close(fd);
        fd = -1;
        // TODO: if there packets to be send, switch to pending
        // TODO: have one method to switch the state and perform the needed actions (e.g. handle timers)?
        state = ConnectionState::IDLE;
        return;
    }
    println("TcpConnection::canRead(): state = {}", std::to_underlying(state));
    if (state == ConnectionState::INPROGRESS) {
        state = ConnectionState::ESTABLISHED;
    }
    println("recv'd {} bytes", nbytes);
    recv(buffer, nbytes);
}

void TcpConnection::up() {
    println("UP CONFIG IS {}", remote.str());
    if (fd >= 0) {
        return;
    }

    fd = connect_to(remote.host.c_str(), remote.port);
    if (fd < 0) {
        println("UP PENDING");
        state = ConnectionState::PENDING;
        throw runtime_error(format("TcpConnection()::up(): {}: {}", remote.str(), strerror(errno)));
    }
    if (errno == EINPROGRESS) {
        println("UP INPROGRESS");
        state = ConnectionState::INPROGRESS;
        startWriteHandler();
    } else {
        println("UP ESTABLISHED");
        state = ConnectionState::ESTABLISHED;
    }
    startReadHandler();
}

// called by libev when a client want's to connect
void libev_accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    // println("incoming: got client");
    // puts("got client");
    if (EV_ERROR & revents) {
        perror("got invalid event");
        return;
    }

    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);
    int fd = accept(watcher->fd, (struct sockaddr *)&addr, &addrlen);

    println("ACCEPT LOCAL {}, REMOTE {}", getLocalName(fd).str(), getPeerName(fd).str());

    if (set_non_block(fd) == -1 || set_no_delay(fd) == -1) {
        puts("failed to setup");
        close(fd);
        return;
    }

    auto handler = reinterpret_cast<listen_handler_t *>(watcher);
    auto peer = getPeerName(fd);
    auto connection = make_shared<TcpConnection>(handler->protocol, peer.host.c_str(), peer.port);
    handler->protocol->pool.insert(connection);
    connection->accept(fd);
}

// called by libev when data can be read
void libev_read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    if (EV_ERROR & revents) {
        perror("libev_read_cb(): got invalid event");
        return;
    }
    auto handler = reinterpret_cast<read_handler_t *>(watcher);
    handler->connection->canRead();
}

static void libev_write_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    ev_io_stop(loop, watcher);
    auto handler = reinterpret_cast<write_handler_t *>(watcher);
    handler->connection->canWrite();
}

void TcpConnection::stopReadHandler() {
    if (readHandler) {
        ev_io_stop(protocol->loop, &readHandler->watcher);
        readHandler.reset();
    }
}

void TcpConnection::stopWriteHandler() {
    if (writeHandler) {
        ev_io_stop(protocol->loop, &writeHandler->watcher);
        writeHandler.reset();
    }
}

void TcpConnection::startReadHandler() {
    if (readHandler) {
        return;
    }
        readHandler = make_unique<read_handler_t>();
        readHandler->connection = this;
        ev_io_init(&readHandler->watcher, libev_write_cb, fd, EV_WRITE);
        ev_io_start(protocol->loop, &readHandler->watcher);
}

void TcpConnection::startWriteHandler() {
    if (writeHandler) {
        return;
    }
        writeHandler = make_unique<write_handler_t>();
        writeHandler->connection = this;
        ev_io_init(&writeHandler->watcher, libev_write_cb, fd, EV_WRITE);
        ev_io_start(protocol->loop, &writeHandler->watcher);
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

TcpConnection *ConnectionPool::find(const char *host, uint16_t port) {
    for (auto &c : connections) {
        if (c->remote.host == host && c->remote.port == port) {
            return c.get();
        }
    }
    return nullptr;
    // auto conn = make_shared<TcpConnection>(nullptr, host, port);
    // auto p = connections.find(conn);
    // if (p == connections.end()) {
    //     return nullptr;
    // }
    // return p->get();
}

void ConnectionPool::print() {
    for (auto c : connections) {
        c->print();
    }
}
