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
#include "interface/interface_impl.hh"
#include "interface/interface_skel.hh"
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
    describe("networking", [] {
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

                expect(pool->find("a", 80).get()).to.equal(a80.get());
                expect(pool->find("b", 79).get()).to.equal(b79.get());
                expect(pool->find("b", 80).get()).to.equal(b80.get());
                expect(pool->find("b", 81).get()).to.equal(b81.get());
                expect(pool->find("c", 80).get()).to.equal(c80.get());

                pool->erase(b80);

                expect(pool->find("a", 80).get()).to.equal(a80.get());
                expect(pool->find("b", 79).get()).to.equal(b79.get());
                expect(pool->find("b", 80).get()).to.equal(nullptr);
                expect(pool->find("b", 81).get()).to.equal(b81.get());
                expect(pool->find("c", 80).get()).to.equal(c80.get());

                pool->erase(a80);
                pool->erase(b79);
                pool->erase(b81);
                pool->erase(c80);
            });
        });
        describe("IIOPStream2Packet", [] {
            it("IIOPStream2Packet(readBufferSize) will provide a read buffer of at least readBufferSize bytes", [] {
                IIOPStream2Packet s2p(512);
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

                // WHEN it's feed into IIOPStream2Packet
                IIOPStream2Packet s2p(512);
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

                // WHEN it's feed into IIOPStream2Packet 3 times
                IIOPStream2Packet s2p(512);
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
                IIOPStream2Packet s2p(512);
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
                auto clientConn = client->connectOutgoing("mark13.org", 2809);

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
                auto clientConn = client->connectOutgoing("127.0.0.1", 9090);

                clientConn->up();
                expect(clientConn->state).to.equal(ConnectionState::INPROGRESS);

                ev_run(loop, 0);
                expect(clientConn->state).to.equal(ConnectionState::IDLE);
                // expect(system("/sbin/iptables -v -L INPUT")).to.equal(0);
            });

            it("handle large amounts of outgoing and incoming data", [] {
                setbuf(stdout, nullptr);
                {
                struct ev_loop *loop = EV_DEFAULT;

                auto serverORB = make_shared<CORBA::ORB>("server");
                serverORB->debug = true;
                auto serverProto = new CORBA::detail::TcpProtocol(loop);
                serverORB->registerProtocol(serverProto);
                serverProto->listen("127.0.0.1", 9003);

                serverORB->bind("Backend", make_shared<Interface_impl>());

                auto clientORB = make_shared<CORBA::ORB>("client");
                clientORB->debug = true;
                auto clientProto = new CORBA::detail::TcpProtocol(loop);
                clientORB->registerProtocol(clientProto);

                std::exception_ptr eptr;
                parallel(eptr, loop, [clientORB] -> async<> {
                    // expect(clientORB.use_count()).to.equal(1);
                    println("clientORB.use_count() = {}: {}:{}", clientORB.use_count(), __FILE__, __LINE__);
                    // this resolves the object id
                    auto object = co_await clientORB->stringToObject("corbaname::127.0.0.1:9003#Backend");
                    println("clientORB.use_count() = {}: {}:{}", clientORB.use_count(), __FILE__, __LINE__);
                    // this creates the stub
                    auto backend = Interface::_narrow(object);
                    println("clientORB.use_count() = {}: {}:{}", clientORB.use_count(), __FILE__, __LINE__);

                    auto l = 0x100000;

                    println("===== send 26 packets");

                    for (int i = 0; i < 26; ++i) {
                        CORBA::GIOPEncoder encoder;
                        encoder.encodeRequest(CORBA::blob("1234"), "operation", 0, false);
                        string s(l, 'a' + i);
                        backend->recvString(s);
                    }

                    println("===== receive 26 packets");
                    // recvString is a oneway method, so we call this to wait for all messages being parsed
                    co_await backend->callString("wait");
                    backend = nullptr;
                });

                ev_run(loop, 0);

                if (eptr) {
                    std::rethrow_exception(eptr);
                }

                serverORB->shutdown();
                clientORB->shutdown();
                expect(serverORB.use_count()).to.equal(1);
                expect(clientORB.use_count()).to.equal(1);

                }
                println("====================================================================================");


                // TODO: check that the received data is correct
            });

            // FIXME: heap use after free
            xit("server restarts", [] {
                struct ev_loop *loop = EV_DEFAULT;

                auto serverORB = make_shared<CORBA::ORB>("server");
                // serverORB->debug = true;
                auto serverProto = new CORBA::detail::TcpProtocol(loop);
                serverORB->registerProtocol(serverProto);
                serverProto->listen("127.0.0.1", 9003);

                serverORB->bind("Backend", make_shared<Interface_impl>());

                auto clientORB = make_shared<CORBA::ORB>("client");
                clientORB->debug = true;
                auto clientProto = new CORBA::detail::TcpProtocol(loop);
                clientORB->registerProtocol(clientProto);

                std::exception_ptr eptr;
                parallel(eptr, loop, [&] -> async<> {
                    auto object = co_await clientORB->stringToObject("corbaname::127.0.0.1:9003#Backend");
                    auto backend = Interface::_narrow(object);

                    co_await backend->callString("one");

                    println("======================== close 1st server orb");
                    // this might crash because we're mixing async too much?
                    serverORB->shutdown();
                    println("=============================================");

                    auto serverORB2 = make_shared<CORBA::ORB>("server");
                    serverORB2->debug = true;
                    auto serverProto2 = new CORBA::detail::TcpProtocol(loop);
                    serverORB2->registerProtocol(serverProto2);
                    serverProto2->listen("127.0.0.1", 9003);

                    co_await backend->callString("two");
                });

                ev_run(loop, 0);

                if (eptr) {
                    std::rethrow_exception(eptr);
                }
            });

            it("throw TRANSIENT exception when initial connection to peer fails", [] {
                auto clientORB = make_shared<CORBA::ORB>("client");
                // clientORB->debug = true;

                struct ev_loop *loop = EV_DEFAULT;
                auto clientProto = new CORBA::detail::TcpProtocol(loop);
                clientORB->registerProtocol(clientProto);

                std::exception_ptr eptr;
                parallel(eptr, loop, [&] -> async<> {
                    auto object = co_await clientORB->stringToObject("corbaname::127.0.0.1:9003#Backend");
                    auto backend = Interface::_narrow(object);
                    co_await backend->callString("one");
                });
                ev_run(loop, 0);

                expect(eptr).to.beTrue();
                expect([&] { std::rethrow_exception(eptr); })
                    .to.throw_(CORBA::TRANSIENT(0, CORBA::CompletionStatus::YES));
            });

            it("throw TIMEOUT exception when initial connection to peer is not accepted in time", [] {
                auto fds = create_listen_socket("127.0.0.1", 9003);

                auto clientORB = make_shared<CORBA::ORB>("client");
                clientORB->debug = true;

                struct ev_loop *loop = EV_DEFAULT;
                auto clientProto = new CORBA::detail::TcpProtocol(loop);
                clientORB->registerProtocol(clientProto);

                std::exception_ptr eptr;
                parallel(eptr, loop, [&] -> async<> {
                    auto object = co_await clientORB->stringToObject("corbaname::127.0.0.1:9003#Backend");
                    auto backend = Interface::_narrow(object);
                    co_await backend->callString("one");
                });
                ev_run(loop, 0);

                expect(eptr).to.beTrue();
                expect([&] { std::rethrow_exception(eptr); })
                    .to.throw_(CORBA::TIMEOUT(0, CORBA::CompletionStatus::YES));

                for(auto &fd: fds) {
                    close(fd);
                }
            });

            // scenarios to test:
            // * what happens when we have a drop rule?
            //   * before the connection comes up
            //   * when the connection is idle
            //   * when we try to send
            // * other rules than the drop rule
            // * concurrent operations

            // what exceptions are thrown when?

            // https://users.cs.northwestern.edu/~agupta/cs340/project2/TCPIP_State_Transition_Diagram.pdf
            // SYN
            // ACK
            // FIN: last packet from sender
            // RST: reset

            // how a TCP connection is opened
            // SYN ->
            // <- SYN-ACK
            // ACK ->

            // how a TCP connection is closed
            // FIN ->
            // <- ACK
            // <- FIN
            // -> ACK

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
