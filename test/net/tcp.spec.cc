#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstddef>

#include <fstream>

#include "../interface/interface_impl.hh"
#include "../interface/interface_skel.hh"
#include "../src/corba/corba.hh"
#include "../src/corba/net/tcp/protocol.hh"
#include "../src/corba/net/tcp/connection.hh"
#include "../util.hh"
#include "kaffeeklatsch.hh"

using namespace kaffeeklatsch;
using namespace std;
using CORBA::async;

std::string readString(const char *filename) {
    std::ifstream t(filename);
    t.seekg(0, std::ios::end);
    size_t size = t.tellg();
    std::string buffer(size, ' ');
    t.seekg(0);
    t.read(&buffer[0], size);
    return buffer;
}

void libev_read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
}

kaffeeklatsch_spec([] {
    describe("net", [] {
        describe("tcp", [] {
            it("bi-directional iiop connection", [] {
                struct ev_loop *loop = EV_DEFAULT;

                auto serverORB = make_shared<CORBA::ORB>("server");
                serverORB->debug = true;
                auto serverProto = new CORBA::detail::TcpProtocol(loop);
                serverORB->registerProtocol(serverProto);
                serverProto->listen("127.0.0.1", 9003);

                auto backend = make_shared<Interface_impl>(serverORB);
                serverORB->bind("Backend", backend);

                std::exception_ptr eptr;

                auto clientORB = make_shared<CORBA::ORB>("client");
                clientORB->debug = true;
                auto clientProto = new CORBA::detail::TcpProtocol(loop);
                clientORB->registerProtocol(clientProto);

                parallel(eptr, loop, [clientORB] -> async<> {

                    println("CLIENT: resolve 'Backend'");
                    auto object = co_await clientORB->stringToObject("corbaname::127.0.0.1:9003#Backend");
                    auto backend = Interface::_narrow(object);
                    println("CLIENT: call backend");

                    auto frontend = make_shared<Peer_impl>();
                    clientORB->activate_object(frontend);
                    co_await backend->setPeer(frontend);
                    expect(co_await backend->callPeer("hello")).to.equal("hello to the world.");

                    println("CLIENT: YOOOOOOOOOOOOOOOOOOOOOOOOOOO");                    
                });

                println("START LOOP");
                ev_run(loop, 0);

                if (eptr) {
                    std::rethrow_exception(eptr);
                }

                // expect(clientORB->connections.size()).to.equal(1);
                // auto clientConn = clientORB->connections.front();
                // println("CLIENT HAS ONE CONNECTION FROM {}:{} TO {}:{}", clientConn->localAddress(), clientConn->localPort(), clientConn->remoteAddress(),
                //         clientConn->remotePort());
                // expect(clientConn->remoteAddress()).to.equal("localhost");
                // expect(clientConn->remotePort()).to.equal(9003);

                // expect(clientConn->localAddress().c_str()).to.be.uuid();
                // expect(clientConn->localPort()).to.be.not_().equal(0);

                // // localAddress should be a UUID, localPort not 0 (a regex string matched would be nice...)

                // // server should also have a connection
                // expect(serverORB->connections.size()).to.equal(1);
                // auto serverConn = serverORB->connections.front();
                // println("SERVER HAS ONE CONNECTION FROM {}:{} TO {}:{}", serverConn->localAddress(), serverConn->localPort(), serverConn->remoteAddress(),
                //         serverConn->remotePort());
                // expect(serverConn->localAddress()).to.equal("localhost");
                // expect(serverConn->localPort()).to.equal(9003);

                // expect(clientConn->localAddress()).to.equal(serverConn->remoteAddress());
                // expect(clientConn->localPort()).to.equal(serverConn->remotePort());
            });
            xit("call omni orb", [] {
                struct ev_loop *loop = EV_DEFAULT;

                auto orb = make_shared<CORBA::ORB>();
                auto protocol = new CORBA::detail::TcpProtocol(loop);
                orb->registerProtocol(protocol);
                // orb->debug = true;

                // BiDirIIOP does not work yet, so we must offer a server port for the server be able to call us
                protocol->listen("192.168.178.24", 9003);
                // protocol->attach(clientORB.get(), loop);

                std::exception_ptr eptr;

                // client must be created outside of parallel() because it must live longer
                println("create client");
                auto client = make_shared<Client_impl>();
                orb->activate_object(client);

                parallel(eptr, loop, [orb, client] -> async<> {
                    try {
                        println("resolve ior");
                        auto object = co_await orb->stringToObject(readString("IOR.txt"));
                        println("narrow server");
                        auto server = Server::_narrow(object);
                        // println("create client");
                        // auto client = make_shared<Client_impl>(clientORB);
                        println("register client");
                        server->addClient(client);
                        co_return;
                    } catch (std::exception &ex) {
                        println("client caught exception: {}", ex.what());
                    }
                });

                println("start loop");
                while (true) {
                    ev_run(loop, EVRUN_ONCE);
                    if (eptr) {
                        std::rethrow_exception(eptr);
                    }
                }
            });
            xit("find all the system's ip addresses", [] {

                struct ev_loop *loop = EV_DEFAULT;

                auto orb = make_shared<CORBA::ORB>();
                auto protocol = new CORBA::detail::TcpProtocol(loop);
                orb->registerProtocol(protocol);
                orb->debug = true;

                protocol->listen("0.0.0.0", 9003);

                ifaddrs *addrs, *tmp;
                getifaddrs(&addrs);
                tmp = addrs;
                while (tmp) {
                    if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET) {
                        sockaddr_in *addr = reinterpret_cast<sockaddr_in *>(tmp->ifa_addr);
                        printf("%s %s:%u\n", tmp->ifa_name, inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));
                    } else
                    if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET6) {
                        sockaddr_in6 *addr = reinterpret_cast<sockaddr_in6 *>(tmp->ifa_addr);
                        char buf[INET6_ADDRSTRLEN];
                        inet_ntop(tmp->ifa_addr->sa_family, &addr->sin6_addr, buf, sizeof(buf));
                        printf("%s %s:%u\n", tmp->ifa_name, buf, ntohs(addr->sin6_port));
                    } else {
                        printf("%s\n", tmp->ifa_name);
                    }
                    tmp = tmp->ifa_next;
                }
                freeifaddrs(addrs);
                // return;
            });
        });
    });
});
