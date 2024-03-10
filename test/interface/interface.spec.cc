#include "interface.hh"

#include "../../fake.hh"
#include "../../util.hh"
#include "interface_skel.hh"
#include "interface_stub.hh"
#include "kaffeeklatsch.hh"

namespace cppasync {

#ifdef _COROUTINE_DEBUG
unsigned promise_sn_counter = 0;
unsigned async_sn_counter = 0;
unsigned awaitable_sn_counter = 0;
unsigned promise_use_counter = 0;
unsigned async_use_counter = 0;
unsigned awaitable_use_counter = 0;
#endif

}  // namespace cppasync

using namespace std;
using namespace kaffeeklatsch;
using CORBA::async, CORBA::ORB, CORBA::blob, CORBA::blob_view;

class Interface_impl : public Interface_skel {
    public:
        Interface_impl(std::shared_ptr<CORBA::ORB> orb) : Interface_skel(orb) {}

        async<bool> callBoolean(bool value) override { co_return value; }
        async<uint8_t> callOctet(uint8_t value) override { co_return value; }  // check uint8_t with real CORBA

        async<uint16_t> callUShort(uint16_t value) override { co_return value; }
        async<uint32_t> callUnsignedLong(uint32_t value) override { co_return value; }
        async<uint64_t> callUnsignedLongLong(uint64_t value) override { co_return value; }

        async<int16_t> callShort(int16_t value) override { co_return value; }
        async<uint32_t> callLong(int32_t value) override { co_return value; }
        async<uint64_t> callLongLong(int64_t value) override { co_return value; }

        async<float> callFloat(float value) override { co_return value; }
        async<double> callDouble(double value) override { co_return value; }

        async<string> callString(const string_view &value) override { co_return string(value); }
        async<blob> callBlob(const blob_view &value) override { co_return blob(value); }

        std::shared_ptr<Peer> peer;
        async<void> setPeer(std::shared_ptr<Peer> aPeer) override {
            this->peer = aPeer;
            co_return;
        }
        async<std::string> callPeer(const std::string_view &value) override {
            println("INTERFACE IMPL: RECEIVED callPeer(\"{}\"): call peer", value);
            auto s = co_await peer->callString(string(value) + " to the");
            // value is not valid anymore
            println("INTERFACE IMPL: RECEIVED callPeer(...): peer returned \"{}\", return value \"{}.\"", s, s);
            co_return s + ".";
        }
        // next steps:
        // [X] set/get callback object and call it
        // [ ] use ArrayBuffer/Buffer for sequence<octet> for the javascript side
        // [ ] completeness: signed & floating point
};

class Peer_impl : public Peer_skel {
    public:
        Peer_impl(std::shared_ptr<CORBA::ORB> orb) : Peer_skel(orb) {}
        async<string> callString(const string_view &value) override { co_return string(value) + " world"; }
};

kaffeeklatsch_spec([] {
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

                auto frontend = make_shared<Peer_impl>(clientORB);
                co_await backend->setPeer(frontend);
                expect(co_await backend->callPeer("hello")).to.equal("hello to the world.");
            });

            vector<FakeTcpProtocol *> protocols = {serverProtocol, clientProtocol};
            while (transmit(protocols))
                ;

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

            vector<FakeTcpProtocol *> protocols = {serverProtocol, clientProtocol};
            {
                auto packet = clientProtocol->packets.front();
                // this should contain the bidiriiop payload
                transmit(protocols); // client to server
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