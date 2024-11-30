#include <arpa/inet.h>
#include <errno.h>
#include <ev.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <list>
#include <print>
#include <set>
#include <thread>
#include <utility>

#include "../src/corba/exception.hh"
#include "../src/corba/giop.hh"
#include "../src/corba/net/stream2packet.hh"
#include "../src/corba/net/tcp/connection.hh"
#include "../src/corba/net/tcp/protocol.hh"
#include "../src/corba/net/util/socket.hh"
#include "../src/corba/orb.hh"
#include "util.hh"

template <typename T>
bool operator==(const std::span<T> &lhs, const std::span<T> &rhs) {
    auto result = lhs.size_bytes() == rhs.size_bytes() || memcmp(lhs.data(), rhs.data(), lhs.size_bytes()) == 0;
    // std::println("compare {} {} -> {}", lhs.size_bytes(), lhs.size_bytes(), result);
    // if (!result) {
    //     hexdump(lhs.data(), lhs.size_bytes());
    //     hexdump(rhs.data(), rhs.size_bytes());
    // }
    return result;
}

#include "kaffeeklatsch.hh"

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

using namespace std;
using namespace kaffeeklatsch;
using namespace CORBA;
using namespace CORBA::detail;

unique_ptr<vector<char>> str2vec(const char *data) {
    auto vec = make_unique<vector<char>>();
    vec->assign(data, data + strlen(data));
    return vec;
}

kaffeeklatsch_spec([] {
    fdescribe("networking", [] {
        describe("ConnectionPool", [] {
            it("insert, find, erase", [] {
                auto pool = make_unique<CORBA::detail::ConnectionPool>();
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

                pool->erase(b80);

                expect(pool->find("a", 80)).to.equal(a80.get());
                expect(pool->find("b", 79)).to.equal(b79.get());
                expect(pool->find("b", 80)).to.equal(nullptr);
                expect(pool->find("b", 81)).to.equal(b81.get());
                expect(pool->find("c", 80)).to.equal(c80.get());

                pool->erase(a80);
                pool->erase(b79);
                pool->erase(b81);
                pool->erase(c80);
            });
        });
        describe("GIOPStream2Packets", [] {
            it("GIOPStream2Packets(readBufferSize) will provide a read buffer of at least readBufferSize bytes", [] {
                GIOPStream2Packets s2p(512);
                expect(s2p.buffer()).to.not_().equal(nullptr);
                expect(s2p.length()).to.equal(512);
            });
            it("returns a single packet as is", [] {
                // GIVEN a whole single packet being read
                CORBA::GIOPEncoder encoder;
                encoder.encodeRequest(CORBA::blob("1234"), "operation", 0, false);
                string s(16, 'a');
                encoder.writeString(s);
                encoder.setGIOPHeader(CORBA::MessageType::REQUEST);

                // WHEN it's feed into GIOPStream2Packets
                GIOPStream2Packets s2p(512);
                memcpy(s2p.buffer(), encoder.buffer.data(), encoder.buffer.length());

                s2p.received(encoder.buffer.length());

                // THEN it's returned on the 1st call to message()
                expect(span(encoder.buffer.data(), encoder.buffer.length())).to.equal(s2p.message());

                // THEN the 2nd call to message() returns an empty
                expect(s2p.message().empty()).to.beTrue();
            });
            it("returns 3 packets", [] {
                CORBA::GIOPEncoder encoder;
                encoder.encodeRequest(CORBA::blob("1234"), "operation", 0, false);
                string s(19, 'a');
                encoder.writeString(s);
                encoder.setGIOPHeader(CORBA::MessageType::REQUEST);

                // WHEN it's feed into GIOPStream2Packets 3 times
                GIOPStream2Packets s2p(512);
                memcpy(s2p.buffer(), encoder.buffer.data(), encoder.buffer.length());
                memcpy(s2p.buffer() + encoder.buffer.length(), encoder.buffer.data(), encoder.buffer.length());
                memcpy(s2p.buffer() + 2 * encoder.buffer.length(), encoder.buffer.data(), encoder.buffer.length());

                s2p.received(3 * encoder.buffer.length());

                // hexdump(s2p.data, s2p.size);

                // THEN it's returned on the 1st, 2nd and 3rd call to message()
                // println("========== m0");
                auto m0 = s2p.message();
                // println("m0 {} {}", m0.data() - s2p.data, m0.size_bytes());
                expect(span(encoder.buffer.data(), encoder.buffer.length())).to.equal(m0);

                // println("========== m1");
                auto m1 = s2p.message();
                // println("m1 {} {}", m1.data() - s2p.data, m1.size_bytes());
                expect(span(encoder.buffer.data(), encoder.buffer.length())).to.equal(m1);

                // println("========== m2");
                auto m2 = s2p.message();
                // println("m1 {} {}", m2.data() - s2p.data, m2.size_bytes());
                expect(span(encoder.buffer.data(), encoder.buffer.length())).to.equal(m2);

                // THEN the 4th call to message() returns an empty
                // println("========== m3");
                auto m3 = s2p.message();
                expect(m3.empty()).to.beTrue();
            });
            it("can handle incomplete packets", [] {
                CORBA::GIOPEncoder encoder;
                encoder.encodeRequest(CORBA::blob("1234"), "operation", 0, false);
                string s(19, 'a');
                encoder.writeString(s);
                encoder.setGIOPHeader(CORBA::MessageType::REQUEST);

                // WHEN we received one and a partial packet
                GIOPStream2Packets s2p(512);
                auto buffer = s2p.buffer();
                memcpy(buffer, encoder.buffer.data(), encoder.buffer.length());
                memcpy(buffer + encoder.buffer.length(), encoder.buffer.data(), 16);

                s2p.received(encoder.buffer.length() + 16);

                // hexdump(s2p.data, s2p.size);

                // THEN the 1st call to message() returns the first message
                // println("========== m0");
                auto m0 = s2p.message();
                // println("m0 {} {}", m0.data() - s2p.data, m0.size_bytes());
                expect(span(encoder.buffer.data(), encoder.buffer.length())).to.equal(m0);

                // THEN the 2nd call to message() returns an empty
                // println("========== m1");
                auto m1 = s2p.message();
                expect(m1.empty()).to.beTrue();

                // WHEN we received the remaining packet
                buffer = s2p.buffer();
                memcpy(buffer, encoder.buffer.data() + 16, encoder.buffer.length() - 16);
                s2p.received(encoder.buffer.length() - 16);

                // THEN the 3rd call to message() returns the second message
                // println("========== m2");
                auto m2 = s2p.message();
                // println("m2 {} {}", m0.data() - s2p.data, m0.size_bytes());
                expect(span(encoder.buffer.data(), encoder.buffer.length())).to.equal(m2);

                // THEN the 4th call to message() returns an empty
                // println("========== m3");
                auto m3 = s2p.message();
                expect(m3.empty()).to.beTrue();
            });

            // splits two packets
        });
        describe("TcpProtocol::listen(host, port)", [] {
            it("when the socket is already in use, it throws a CORBA::INITIALIZE exception", [] {
                struct ev_loop *loop = EV_DEFAULT;
                auto protocol0 = make_unique<TcpProtocol>(loop);

                expect(protocol0->loop).to.equal(loop);

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
                auto clientConn = client->connect("mark13.org", 2809);

                // THEN the connection is for the given remote host and port
                expect(clientConn->remote.host).to.equal("mark13.org");
                expect(clientConn->remote.port).to.equal(2809);
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

            // it("listener, no data transmitted: IDLE -> INPROGRESS -> ESTABLISHED", [] {
            //     struct ev_loop *loop = EV_DEFAULT;

            //     // GIVEN a listening server
            //     auto server = make_unique<TcpProtocol>(loop);
            //     server->listen("127.0.0.1", 9003);

            //     // GIVEN a client
            //     auto client = make_unique<TcpProtocol>(loop);
            //     auto clientConn = client->connect("127.0.0.1", 9003);

            //     // WHEN the client is asked to connect
            //     clientConn->up();

            //     // THEN the connection is in state INPROGRESS
            //     expect(clientConn->state).to.equal(ConnectionState::INPROGRESS);

            //     // WHEN the server accepts the connection
            //     ev_run(loop, EVRUN_ONCE);

            //     // THEN both server and client connections are in state ESTABLISHED
            //     expect(clientConn->state).to.equal(ConnectionState::ESTABLISHED);

            //     TcpConnection *serverConn = nullptr;
            //     while (serverConn == nullptr) {
            //         ev_run(loop, EVRUN_ONCE | EVRUN_NOWAIT);
            //         serverConn = server->pool.find("127.0.0.1", getLocalName(clientConn->getFD()).port);
            //     }
            //     expect(serverConn->state).to.equal(ConnectionState::ESTABLISHED);
            // });

            // xit("when connections are established, data can be transmitted in both directions", [] {
            //     struct ev_loop *loop = EV_DEFAULT;

            //     auto server = make_unique<TcpProtocol>(loop);
            //     server->listen("localhost", 9003);

            //     auto client = make_unique<TcpProtocol>(loop);
            //     auto clientConn = client->connect("localhost", 9003);
            //     clientConn->up();

            //     ev_run(loop, EVRUN_ONCE);
            //     expect(clientConn->state).to.equal(ConnectionState::ESTABLISHED);

            //     auto serverConn = server->pool.find("::1", getLocalName(clientConn->getFD()).port);
            //     expect(serverConn).to.be.not_().equal(nullptr);
            //     expect(serverConn->state).to.equal(ConnectionState::ESTABLISHED);

            //     string serverReceived;
            //     serverConn->receiver = [&](void *buffer, size_t nbytes) {
            //         serverReceived.assign((char *)buffer, nbytes);
            //     };
            //     clientConn->send(str2vec("hello"));

            //     expect(clientConn->state).to.equal(ConnectionState::ESTABLISHED);

            //     ev_run(loop, EVRUN_ONCE);
            //     expect(serverReceived).to.equal("hello");

            //     string clientReceived;
            //     clientConn->receiver = [&](void *buffer, size_t nbytes) {
            //         clientReceived.assign((char *)buffer, nbytes);
            //     };

            //     serverConn->send(str2vec("hello"));
            //     ev_run(loop, EVRUN_ONCE);
            //     expect(clientReceived).to.equal("hello");
            // });
            it("handle large amounts of outgoing and incoming data", [] {
                struct ev_loop *loop = EV_DEFAULT;

                auto sockets = create_listen_socket("127.0.0.1", 9013);
                expect(sockets.size()).to.be.greaterThan(0);
                auto server = sockets[0];

                // auto server = make_unique<TcpProtocol>(loop);
                // server->listen("127.0.0.1", 9003);

                auto client = make_unique<TcpProtocol>(loop);
                auto clientConn = client->connect("127.0.0.1", 9013);
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

                auto l = 0x100000;

                println("===== send 26 packets");

                for (int i = 0; i < 26; ++i) {
                    CORBA::GIOPEncoder encoder;
                    encoder.encodeRequest(CORBA::blob("1234"), "operation", 0, false);
                    string s(l, 'a' + i);
                    encoder.writeString(s);
                    encoder.setGIOPHeader(CORBA::MessageType::REQUEST);  // THIS IS TOTAL BOLLOCKS BECAUSE OF THE RESIZE IN IT...
                    serverRecvBufSize = encoder.buffer.length();
                    clientConn->send(std::move(encoder.buffer._data));
                    ev_run(loop, EVRUN_ONCE | EVRUN_NOWAIT);
                }

                println("===== receive 26 packets");

                size_t receivedTotal = 0;
                GIOPStream2Packets g2p;
                while (true) {
                    // println("server: before recv (total {}, want {})", receivedTotal, (26 * serverRecvBufSize));
                    ssize_t n = recv(serverConn, g2p.buffer(), g2p.length(), 0);
                    if (n >= 0) {
                        g2p.received(n);
                        while (!g2p.message().empty()) {
                            // println("got message");
                        }
                        receivedTotal += n;
                        // println("server: got {} (total {}, want {})", n, receivedTotal, (26 * serverRecvBufSize));
                        if (receivedTotal == 26 * serverRecvBufSize) {
                            break;
                        }
                    } else {
                        if (errno == EAGAIN) {
                            ev_run(loop, EVRUN_ONCE | EVRUN_NOWAIT);
                            // sleep(1);
                        } else {
                            println("server: got error: {} ({})", strerror(errno), errno);
                        }
                    }
                    ev_run(loop, EVRUN_ONCE | EVRUN_NOWAIT);
                    // hexdump(in, n);
                }

                // this covers the general case, sometimes it might have an incomplete send
                // for testing send incomplete send, we could use a flag to force a smaller send
                // BETTER: send more than fits into max of SO_RCVBUF and SO_SNDBUF

                // for re-assembling: the GIOP header is read in MessageType GIOPDecoder::scanGIOPHeader(),
                // has 16 bytes and contains the lenght of the whole message.
                // for testing, just use a small receive buffer... my previous implementation had a hardcoded 8kb

                // when reading, begin with a buffer of size SO_RCVBUF
            });

            // fit("listener disappears", [] {
            //     struct ev_loop *loop = EV_DEFAULT;

            //     // GIVEN a listening server
            //     auto server = make_unique<TcpProtocol>(loop);
            //     server->listen("127.0.0.1", 9003);

            //     // GIVEN a client
            //     auto client = make_unique<TcpProtocol>(loop);
            //     auto clientConn = client->connect("127.0.0.1", 9003);

            //     // WHEN the client is asked to connect
            //     clientConn->up();
            //     ev_run(loop, EVRUN_ONCE);
            //     expect(clientConn->state).to.equal(ConnectionState::ESTABLISHED);

            //     TcpConnection *serverConn = nullptr;
            //     while (serverConn == nullptr) {
            //         ev_run(loop, EVRUN_ONCE | EVRUN_NOWAIT);
            //         serverConn = server->pool.find("127.0.0.1", getLocalName(clientConn->getFD()).port);
            //     }
            //     expect(serverConn->state).to.equal(ConnectionState::ESTABLISHED);

            //     println("=======================================");

            //     // i guess, unless we try to send and fail or don't get a reply in time, we won't notice...

            //     ignore_sig_pipe();

            //     server.reset();
            //     // ev_run(loop, EVRUN_ONCE);

            //     for (int i = 0; i < 5; ++i) {
            //         sleep(1);
            //         clientConn->send(str2vec("hello"));
            //         ev_run(loop, EVRUN_ONCE | EVRUN_NOWAIT);
            //     }
            //     println("=======================================");

            //     server = make_unique<TcpProtocol>(loop);
            //     server->listen("127.0.0.1", 9003);
            //     for (int i = 0; i < 10; ++i) {
            //         sleep(1);
            //         clientConn->send(str2vec("hello"));
            //         ev_run(loop, EVRUN_ONCE | EVRUN_NOWAIT);
            //     }

            //     println("=======================================");
            // });

            it("listener disappears, comes back again");

            it("retry until listener is ready");

            // ConnectionPool needs a map, TcpProtocol needs to own the connections
            // connection can have different remote host:port names
            // * the a hostname used during connect
            // * the remote ip we see when the connection is established
            // * the remote name we've got during IIOP

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
