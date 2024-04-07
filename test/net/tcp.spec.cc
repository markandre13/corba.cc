#include "../src/corba/net/tcp.hh"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "../interface/interface_impl.hh"
#include "../interface/interface_skel.hh"
#include "../util.hh"
#include "../src/corba/corba.hh"
#include "kaffeeklatsch.hh"

using namespace kaffeeklatsch;
using namespace std;
using CORBA::async;

kaffeeklatsch_spec([] {
    describe("net", [] {
        describe("tcp", [] {
            fit("bi-directional iiop connection", [] {
                struct ev_loop *loop = EV_DEFAULT;

                // start server & client on the same ev loop
                auto serverORB = make_shared<CORBA::ORB>();
                serverORB->debug = true;
                auto protocol = new CORBA::net::TcpProtocol();
                serverORB->registerProtocol(protocol);
                protocol->listen(serverORB.get(), loop, "localhost", 9002);

                auto backend = make_shared<Interface_impl>(serverORB);
                serverORB->bind("Backend", backend);

                std::exception_ptr eptr;

                auto clientORB = make_shared<CORBA::ORB>();

                parallel(eptr, loop, [loop, clientORB] -> async<> {
                    auto protocol = new CORBA::net::TcpProtocol();
                    clientORB->registerProtocol(protocol);
                    clientORB->debug = true;

                    protocol->attach(clientORB.get(), loop);

                    println("CLIENT: resolve 'Backend'");
                    auto object = co_await clientORB->stringToObject("corbaname::localhost:9002#Backend");
                    auto backend = co_await Interface::_narrow(object);
                    println("CLIENT: call backend");

                    auto frontend = make_shared<Peer_impl>(clientORB);
                    co_await backend->setPeer(frontend);
                    expect(co_await backend->callPeer("hello")).to.equal("hello to the world.");
                });

                println("START LOOP");
                ev_run(loop, 0);

                if (eptr) {
                    std::rethrow_exception(eptr);
                }

                expect(clientORB->connections.size()).to.equal(1);
                auto clientConn = clientORB->connections.front();
                println("CLIENT HAS ONE CONNECTION FROM {}:{} TO {}:{}", clientConn->localAddress(), clientConn->localPort(), clientConn->remoteAddress(), clientConn->remotePort());
                expect(clientConn->remoteAddress()).to.equal("localhost");
                expect(clientConn->remotePort()).to.equal(9002);
                
                expect(clientConn->localAddress().c_str()).to.be.uuid();
                expect(clientConn->localPort()).to.be.not_().equal(0);

                // localAddress should be a UUID, localPort not 0 (a regex string matched would be nice...)

                // server should also have a connection
                expect(serverORB->connections.size()).to.equal(1);
                auto serverConn = serverORB->connections.front();
                println("SERVER HAS ONE CONNECTION FROM {}:{} TO {}:{}", serverConn->localAddress(), serverConn->localPort(), serverConn->remoteAddress(), serverConn->remotePort());
                expect(serverConn->localAddress()).to.equal("localhost");
                expect(serverConn->localPort()).to.equal(9002);

                expect(clientConn->localAddress()).to.equal(serverConn->remoteAddress());
                expect(clientConn->localPort()).to.equal(serverConn->remotePort());
            });
        });
    });
});