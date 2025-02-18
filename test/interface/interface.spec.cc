#include "../fake.hh"
#include "../util.hh"
#include "../src/corba/util/logger.hh"
#include "interface_impl.hh"
#include "kaffeeklatsch.hh"

using namespace std;
using namespace kaffeeklatsch;
using CORBA::async, CORBA::ORB, CORBA::blob, CORBA::blob_view;

bool operator==(const RGBA& lhs, const RGBA& rhs) { return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a; }

kaffeeklatsch_spec([] {
    describe("interface", [] {
        fit("send'n receive", [] {
            Logger::setDestination(nullptr);
            Logger::setLevel(LOG_DEBUG);

            // SERVER
            auto serverORB = make_shared<ORB>();
            auto serverProtocol = new FakeTcpProtocol(serverORB.get(), "backend.local", 2809);
            serverORB->registerProtocol(serverProtocol);
            // serverORB->debug = true;
            auto serverImpl = make_shared<Interface_impl>(serverORB);
            serverORB->bind("Backend", serverImpl);

            // CLIENT
            auto clientORB = make_shared<ORB>();
            // clientORB->debug = true;
            auto clientProtocol = new FakeTcpProtocol(clientORB.get(), "frontend.local", 32768);
            clientORB->registerProtocol(clientProtocol);

            std::exception_ptr eptr;
            bool done = false;

            parallel(eptr, [&] -> async<> {
                shared_ptr<CORBA::Object> object;
                object = co_await clientORB->stringToObject("corbaname::backend.local:2809#Backend");
                expect(object.get()).to.not_().equal(nullptr);

                auto serverStub = Interface::_narrow(object);
                expect(serverStub.get()).to.not_().equal(nullptr);

                co_await serverStub->roAttribute();
                // expect(co_await serverStub->roAttribute()).to.equal("static");

                expect(co_await serverStub->rwAttribute()).to.equal("hello");
                co_await serverStub->rwAttribute("world");
                expect(co_await serverStub->rwAttribute()).to.equal("world");

                expect(co_await serverStub->callBoolean(true)).to.equal(true);
                expect(co_await serverStub->callOctet(42)).to.equal(42);

                expect(co_await serverStub->callUShort(65535)).to.equal(65535);
                expect(co_await serverStub->callUnsignedLong(4294967295ul)).to.equal(4294967295ul);
                expect(co_await serverStub->callUnsignedLongLong(18446744073709551615ull)).to.equal(18446744073709551615ull);

                expect(co_await serverStub->callShort(-32768)).to.equal(-32768);
                expect(co_await serverStub->callLong(-2147483648l)).to.equal(-2147483648l);
                expect(co_await serverStub->callLongLong(-9223372036854775807ll)).to.equal(-9223372036854775807ll);

                expect(co_await serverStub->callFloat(3.40282e+38f)).to.equal(3.40282e+38f);
                expect(co_await serverStub->callDouble(4.94066e-324)).to.equal(4.94066e-324);

                expect(co_await serverStub->callString("hello")).to.equal("hello");
                expect(co_await serverStub->callBlob(blob_view("hello"))).to.equal(blob("hello"));

                expect(co_await serverStub->callStruct(RGBA{.r = 255, .g = 192, .b = 128, .a = 64})).to.equal(RGBA{.r = 255, .g = 192, .b = 128, .a = 64});

                float floatArray[] = {3.1415, 2.7182};
                expect(co_await serverStub->callSeqFloat(floatArray)).to.equal(vector<float>(floatArray, floatArray + 2));

                double doubleArray[] = {3.1415l, 2.7182l};
                expect(co_await serverStub->callSeqDouble(doubleArray)).to.equal(vector<double>(doubleArray, doubleArray + 2));

                expect(co_await serverStub->callSeqString({"alice", "bob"})).to.equal({"alice", "bob"});

                vector<RGBA> color{{.r = 255, .g = 192, .b = 128, .a = 64}, {.r = 0, .g = 128, .b = 255, .a = 255}};
                expect(co_await serverStub->callSeqRGBA(color)).to.equal(color);

                auto remoteObjects = co_await serverStub->getRemoteObjects();
                expect(remoteObjects.size()).to.equal(3);
                expect(co_await remoteObjects[0]->id()).to.equal("1");
                expect(co_await remoteObjects[0]->name()).to.equal("alpha");
                expect(co_await remoteObjects[1]->id()).to.equal("2");
                expect(co_await remoteObjects[1]->name()).to.equal("bravo");
                expect(co_await remoteObjects[2]->id()).to.equal("3");
                expect(co_await remoteObjects[2]->name()).to.equal("charly");
                co_await remoteObjects[1]->name("extra! extra!");
                expect(co_await remoteObjects[1]->name()).to.equal("extra! extra!");

                expect(serverImpl->peer.get()).to.equal(nullptr);

                auto frontend = make_shared<Peer_impl>();
                clientORB->activate_object(frontend);
                co_await serverStub->setPeer(frontend);
                expect(serverImpl->peer.get()).to.not_().equal(nullptr);

                println(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
                auto frontendReturned = co_await serverStub->getPeer();
                println("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
                println("{}:{}", __FILE__, __LINE__);
                expect(frontendReturned).is.equal(frontend);
                println("{}:{}", __FILE__, __LINE__);

                expect(co_await serverStub->callPeer("hello")).to.equal("hello to the world.");
                println("{}:{}", __FILE__, __LINE__);

                co_await serverStub->setPeer(nullptr);
                println("{}:{}", __FILE__, __LINE__);
                expect(serverImpl->peer.get()).to.equal(nullptr);
                println("{}:{}", __FILE__, __LINE__);

                done = true;
                println("{}:{}", __FILE__, __LINE__);
            });

            vector<FakeTcpProtocol*> protocols = {serverProtocol, clientProtocol};
            while (transmit(protocols));
            expect(done).to.equal(true);

            if (eptr) {
                std::rethrow_exception(eptr);
            }
        });

        it("iiop", [] {
            // SERVER
            auto serverORB = make_shared<ORB>();
            auto serverProtocol = new FakeTcpProtocol(serverORB.get(), "backend.local", 2809);
            serverORB->registerProtocol(serverProtocol);
            // serverORB->debug = true;
            auto backend = make_shared<Interface_impl>(serverORB);
            serverORB->bind("Backend", backend);

            // CLIENT
            auto clientORB = make_shared<ORB>();
            // clientORB->debug = true;
            auto clientProtocol = new FakeTcpProtocol(clientORB.get(), "frontend.local", 32768);
            clientORB->registerProtocol(clientProtocol);

            std::exception_ptr eptr;

            parallel(eptr, [&clientORB] -> async<> {
                auto object = co_await clientORB->stringToObject("corbaname::backend.local:2809#Backend");
                auto backend = Interface::_narrow(object);

                auto frontend = make_shared<Peer_impl>();
                clientORB->activate_object(frontend);
                co_await backend->setPeer(frontend);
                expect(co_await backend->callPeer("hello")).to.equal("hello to the world.");
            });

            vector<FakeTcpProtocol*> protocols = {serverProtocol, clientProtocol};
            {
                auto packet = clientProtocol->packets.front();
                // this should contain the bidiriiop payload
                transmit(protocols);  // client to server
                // server should have created a connection to client mentioned in bidiriiop payload
                // ws.cc currently uses a hardcoded 'frontend:2'
                // ...
                // the receive must do the lookup, not the protocol
            }

            if (eptr) {
                std::rethrow_exception(eptr);
            }
        });
    });
});