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
#include "kaffeeklatsch.hh"
#include "util.hh"

using namespace std;
using namespace kaffeeklatsch;
using namespace CORBA;
using namespace CORBA::detail;

kaffeeklatsch_spec([] {
    fdescribe("memory", [] {
        describe("ORB", [] {
            it("an ORB has a use count of one", [] {
                auto orb = make_shared<CORBA::ORB>(); 

                expect(orb.use_count()).to.equal(1);
            });
            it("an ORB with a TCP protocol has a use count of one", [] {
                auto orb = make_shared<CORBA::ORB>();

                struct ev_loop *loop = EV_DEFAULT;
                auto protocol = new CORBA::detail::TcpProtocol(loop);
                orb->registerProtocol(protocol);
                protocol->listen("127.0.0.1", 9003);
                // protocol = nullptr;
                expect(orb.use_count()).to.equal(1);

                orb->shutdown();
                expect(orb.use_count()).to.equal(1);
            });
            it("an ORB with a servant has a use count of two", [] {
                auto orb = make_shared<CORBA::ORB>();
                orb->activate_object(make_shared<Interface_impl>(orb));

                expect(orb.use_count()).to.equal(2);

                orb->shutdown();
                expect(orb.use_count()).to.equal(1);
            });
            it("an ORB with servant and a name service has a use count of three", [] {
                auto orb = make_shared<CORBA::ORB>();
                auto backend = make_shared<Interface_impl>(orb);
                orb->activate_object(backend);
                orb->bind("backend", backend);
                backend = nullptr;
                expect(orb.use_count()).to.equal(3);

                orb->shutdown();
                expect(orb.use_count()).to.equal(1);
            });
            it("an ORB with a protocol, a servant and a name service has a use count of three", [] {
                auto orb = make_shared<CORBA::ORB>();

                struct ev_loop *loop = EV_DEFAULT;
                auto protocol = new CORBA::detail::TcpProtocol(loop);
                orb->registerProtocol(protocol);
                protocol->listen("127.0.0.1", 9003);
                // protocol = nullptr;

                auto backend = make_shared<Interface_impl>(orb);
                orb->activate_object(backend);
                orb->bind("backend", backend); // TODO: bind should include the activate_object
                backend = nullptr;
                expect(orb.use_count()).to.equal(3);

                orb->shutdown();
                expect(orb.use_count()).to.equal(1);
            });

        });
    });
});
