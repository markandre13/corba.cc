#include "../fake.hh"
#include "../util.hh"
#include "interface_impl.hh"
#include "kaffeeklatsch.hh"

using namespace std;
using namespace kaffeeklatsch;
using CORBA::async, CORBA::ORB, CORBA::blob, CORBA::blob_view;

bool operator==(const RGBA& lhs, const RGBA& rhs) { return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a; }

kaffeeklatsch_spec([] {
    fdescribe("lifecycle", [] {
        it("connection handling", [] {
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
            });

            vector<FakeTcpProtocol*> protocols = {serverProtocol, clientProtocol};
            while (transmit(protocols));

            if (eptr) {
                std::rethrow_exception(eptr);
            }

        });
    });
    describe("interface", [] {
        it("send'n receive", [] {
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
                auto backend = co_await Interface::_narrow(object);

                expect(co_await backend->roAttribute()).to.equal("static");
                expect(co_await backend->rwAttribute()).to.equal("hello");
                co_await backend->rwAttribute("world");
                expect(co_await backend->rwAttribute()).to.equal("world");

                expect(co_await backend->callBoolean(true)).to.equal(true);
                expect(co_await backend->callOctet(42)).to.equal(42);

                expect(co_await backend->callUShort(65535)).to.equal(65535);
                expect(co_await backend->callUnsignedLong(4294967295ul)).to.equal(4294967295ul);
                expect(co_await backend->callUnsignedLongLong(18446744073709551615ull)).to.equal(18446744073709551615ull);

                expect(co_await backend->callShort(-32768)).to.equal(-32768);
                expect(co_await backend->callLong(-2147483648l)).to.equal(-2147483648l);
                expect(co_await backend->callLongLong(-9223372036854775807ll)).to.equal(-9223372036854775807ll);

                expect(co_await backend->callFloat(3.40282e+38f)).to.equal(3.40282e+38f);
                expect(co_await backend->callDouble(4.94066e-324)).to.equal(4.94066e-324);

                expect(co_await backend->callString("hello")).to.equal("hello");
                expect(co_await backend->callBlob(blob_view("hello"))).to.equal(blob("hello"));

                expect(co_await backend->callStruct(RGBA{.r = 255, .g = 192, .b = 128, .a = 64})).to.equal(RGBA{.r = 255, .g = 192, .b = 128, .a = 64});

                float floatArray[] = {3.1415, 2.7182};
                expect(co_await backend->callSeqFloat(floatArray)).to.equal(vector<float>(floatArray, floatArray + 2));

                double doubleArray[] = {3.1415l, 2.7182l};
                expect(co_await backend->callSeqDouble(doubleArray)).to.equal(vector<double>(doubleArray, doubleArray + 2));

                expect(co_await backend->callSeqString({"alice", "bob"})).to.equal({"alice", "bob"});

                vector<RGBA> color{{.r = 255, .g = 192, .b = 128, .a = 64}, {.r = 0, .g = 128, .b = 255, .a = 255}};
                expect(co_await backend->callSeqRGBA(color)).to.equal(color);

                // omniORB does not establish the tcp connection during _narrow()!!!
                //
                // the connection handling implementation is minimal at the moment.
                // ORB::close() is actually empty!!!
                // proto->create() might need to go...
                //
                // === get parent
                // omniORB: (0) 2024-11-19 08:04:17.825941: sendChunk: to giop:tcp:192.168.178.105:51955 72 bytes
                // omniORB: (3) 2024-11-19 08:04:17.828143: inputMessage: from giop:tcp:192.168.178.105:51955 28 bytes
                // 1.3
                // omniORB: (0) 2024-11-19 08:04:17.844758: inputMessage: from giop:tcp:192.168.178.105:51955 168 bytes
                // omniORB: (0) 2024-11-19 08:04:17.845012: Creating ref to remote: root/BiDirPOA<0>
                //  target id      : IDL:Model:1.0
                //  most derived id: IDL:Model:1.0
                // === call parent
                // omniORB: (0) 2024-11-19 08:04:17.845203: LocateRequest to remote: root/BiDirPOA<0>
                // omniORB: (0) 2024-11-19 08:04:17.845279: sendChunk: to giop:tcp:192.168.178.105:51955 47 bytes
                // omniORB: (0) 2024-11-19 08:04:17.845868: inputMessage: from giop:tcp:192.168.178.105:51955 20 bytes
                // omniORB: (0) 2024-11-19 08:04:17.845939: sendChunk: to giop:tcp:192.168.178.105:51955 76 bytes
                // omniORB: (3) 2024-11-19 08:04:17.849466: inputMessage: from giop:tcp:[::ffff:192.168.178.105]:50768 68 bytes
                // model changed to omniORB: (3) 2024-11-19 08:04:17.849586: sendChunk: to giop:tcp:192.168.178.105:51955 72 bytes
                // omniORB: (0) 2024-11-19 08:04:17.849893: inputMessage: from giop:tcp:192.168.178.105:51955 24 bytes

                auto frontend = make_shared<Peer_impl>(clientORB);
                co_await backend->setPeer(frontend);
                expect(co_await backend->callPeer("hello")).to.equal("hello to the world.");
            });

            vector<FakeTcpProtocol*> protocols = {serverProtocol, clientProtocol};
            while (transmit(protocols));

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
                auto backend = co_await Interface::_narrow(object);

                auto frontend = make_shared<Peer_impl>(clientORB);
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