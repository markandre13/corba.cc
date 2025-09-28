#include "../fake.hh"
#include "../src/corba/util/logger.hh"
#include "../util.hh"
#include "interface_impl.hh"
#include "kaffeeklatsch.hh"

using namespace std;
using namespace kaffeeklatsch;
using CORBA::async, CORBA::ORB, CORBA::blob, CORBA::blob_view;

bool operator==(const RGBA& lhs, const RGBA& rhs) { return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a; }

// for inheritance of interfaces, build everything from the ground up

// https://www.omg.org/spec/CPP/1.3/PDF, p. 126:
//   class A : public virtual Object { ... }
// Some ORB implementations might not use public virtual inheritance from CORBA::Object, and might not make the
// operations pure virtual, but the signatures of the operations will be the same.

// * C++ Language Mapping v1.3, 5.40.3 Inheritance-Based Interface Implementation
// * https://www.omg.org/spec/CPP11 (July 2025)
// * https://www.omg.org/spec/IDL4-CPP (March 2025)
// => the skeleton does not inherit from the interface
//    (then who is inheriting from the interface??? is it for stubs?)

// what if an object is returned by a method?
//   on the server it would return an implementation, on the client a stub?

// interface definition
// CORBA 3.4, Part 1, 8.3 Object Reference Operations, p.104
// class Object {
//         friend class ORB;
//         std::shared_ptr<CORBA::ORB> orb;
//         blob objectKey;
//     public:
//         virtual string_view respository_id() const = 0;
//         std::shared_ptr<CORBA::ORB> get_ORB() { return orb; }
// };

// class A: public virtual Object {
//     public:
//         virtual void sayA() = 0;
//         string_view respository_id() const override;
//         static std::shared_ptr<A> _narrow(std::shared_ptr<CORBA::Object> object);
// };
// class B : public virtual A {
//     public:
//         virtual void sayB() = 0;
//         string_view respository_id() const override;
//         static std::shared_ptr<B> _narrow(std::shared_ptr<CORBA::Object> object);
// };
// class C : public virtual A {
//     public:
//         virtual void sayC() = 0;
//         string_view respository_id() const override;
//         static std::shared_ptr<C> _narrow(std::shared_ptr<CORBA::Object> object);

// };
// class D : public virtual B, public virtual C {
//     public:
//         virtual void sayD() = 0;
//         string_view respository_id() const override;
//         static std::shared_ptr<A> _narrow(std::shared_ptr<CORBA::Object> object);
// };

// string_view A::respository_id() const {
//     static string_view rid("IDL:A:1.0");
//     return rid;
// }
// string_view B::respository_id() const {
//     static string_view rid("IDL:A:1.0");
//     return rid;
// }
// string_view C::respository_id() const {
//     static string_view rid("IDL:A:1.0");
//     return rid;
// }
// string_view D::respository_id() const {
//     static string_view rid("IDL:A:1.0");
//     return rid;
// }

// class Skeleton {
//     virtual CORBA::async<> _call(const std::string_view &operation, CORBA::GIOPDecoder &decoder, CORBA::GIOPEncoder &encoder) = 0;
// };
// class A_skel : public virtual A, virtual Skeleton {
//     CORBA::async<> _call(const std::string_view &operation, CORBA::GIOPDecoder &decoder, CORBA::GIOPEncoder &encoder) override { co_return; }
// };
// class B_skel : public virtual B, public virtual A_skel {
//     CORBA::async<> _call(const std::string_view &operation, CORBA::GIOPDecoder &decoder, CORBA::GIOPEncoder &encoder) override { co_return; }
// };
// class C_skel : public virtual C, public virtual A_skel {
//     CORBA::async<> _call(const std::string_view &operation, CORBA::GIOPDecoder &decoder, CORBA::GIOPEncoder &encoder) override { co_return; }
// };
// class D_skel : public virtual D, public virtual B_skel, public virtual C_skel {
//     CORBA::async<> _call(const std::string_view &operation, CORBA::GIOPDecoder &decoder, CORBA::GIOPEncoder &encoder) override { co_return; }
// };

// class Stub {};
// class A_stub : public virtual A, virtual Stub {
// };
// class B_stub : public virtual B, public virtual A_stub {
// };
// class C_stub : public virtual C, public virtual A_stub {
// };
// class D_stub : public virtual D, public virtual B_stub, public virtual C_stub {
// };

unsigned id = 0;

class A_impl : public virtual A_skel {
    protected:
        unsigned _id;

    public:
        A_impl() : _id(++id) {}
        CORBA::async<void> sayA() override { println("sayA {}", _id); co_return; }
};
class B_impl : public virtual B_skel, public virtual A_impl {
    public:
        CORBA::async<void> sayB() override { println("sayB {}", _id); co_return; }
};
class C_impl : public virtual C_skel, public virtual A_impl {
    public:
        CORBA::async<void> sayC() override { println("sayC {}", _id); co_return; }
};
// class D_impl : public virtual D_skel, public virtual B_impl, public virtual C_impl {
//     public:
//         void sayD() override { println("sayD {}", _id); }
// };

kaffeeklatsch_spec([] {
    
    describe("interface", [] {

        fit("inheritance", [] {
            println("--- A_impl");
            auto a = new A_impl();
            a->sayA().no_wait();
            println("--- B_impl");
            auto b = new B_impl();
            b->sayA().no_wait();
            b->sayB().no_wait();
            println("--- C_impl");
            auto c = new C_impl();
            c->sayA().no_wait();
            c->sayC().no_wait();
            // println("--- D_impl");
            // auto d = new D_impl();
            // d->sayA();
            // d->sayB();
            // d->sayC();
            // d->sayD();
        });

        it("send'n receive", [] {
            // Logger::setDestination(nullptr);
            // Logger::setLevel(LOG_DEBUG);

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
            // FIXME: without 'static' we get a memory violation
            static bool done = false;

            parallel(eptr, [&] -> async<> {
                auto object = co_await clientORB->stringToObject("corbaname::backend.local:2809#Backend");
                expect(object.get()).to.not_().equal(nullptr);

                auto serverStub = Interface::_narrow(object);
                expect(serverStub.get()).to.not_().equal(nullptr);

                co_await serverStub->roAttribute();
                expect(co_await serverStub->roAttribute()).to.equal("static");

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
                // co_await serverStub->setPeer(frontend);
                co_await serverStub->setPeer(frontend);
                expect(serverImpl->peer.get()).to.not_().equal(nullptr);

                // resolve servant
                auto frontendReturned = co_await serverStub->getPeer();
                expect(frontendReturned).is.equal(frontend);

                // call servant
                expect(co_await serverStub->callPeer("hello")).to.equal("hello to the world.");

                // stubs are cached
                auto stub0 = serverImpl->peer.get();
                co_await serverStub->setPeer(frontend);
                auto stub1 = serverImpl->peer.get();
                expect(stub0).to.equal(stub1);

                co_await serverStub->setPeer(nullptr);
                expect(serverImpl->peer.get()).to.equal(nullptr);

                done = true;
            });

            vector<FakeTcpProtocol*> protocols = {serverProtocol, clientProtocol};
            while (transmit(protocols));
            if (eptr) {
                std::rethrow_exception(eptr);
            }
            expect(done).to.equal(true);
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