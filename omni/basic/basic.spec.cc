#include "basic.hh"

#include <omniORB4/minorCode.h>

#include <fstream>
#include <kaffeeklatsch.hh>
#include <print>
#include <thread>

#include <array>

using namespace kaffeeklatsch;
using namespace std;

class Basic_impl : public POA_Basic, public PortableServer::RefCountServantBase {
    public:
        void ping() override;
        void pong() override;
};

kaffeeklatsch_spec([] {
    describe("omniORB tests", [] {
        it("omniORB client calls omniORB server", [] {

            // CREATE SERVER
            array serverArgv = {"basic",
                                // "-ORBtraceLevel", "25",
                                "-ORBclientTransportRule", "* tcp",
                                "-ORBserverTransportRule", "* tcp",
                                "-ORBendPoint", "giop:tcp:127.0.0.1:9002"};
            int argc = serverArgv.size();
            CORBA::ORB_var serverORB = CORBA::ORB_init(argc, const_cast<char**>(serverArgv.data()));

            CORBA::Object_var serverObjPOA = serverORB->resolve_initial_references("RootPOA");
            PortableServer::POA_var serverRootPOA = PortableServer::POA::_narrow(serverObjPOA);
            PortableServer::POAManager_var serverPOAManager = serverRootPOA->the_POAManager();
            serverPOAManager->activate();

            auto impl = new Basic_impl();
            serverRootPOA->activate_object(impl);

            CORBA::String_var ior = serverORB->object_to_string(serverRootPOA->servant_to_reference(impl));

            // OMNI CLIENT
            array clientArgv = {"basic",
                                // "-ORBtraceLevel", "25",
                                "-ORBclientTransportRule", "* tcp",
                                "-ORBserverTransportRule", "* tcp",
                                "-ORBendPoint", "giop:tcp:127.0.0.1:9001"};
            argc = clientArgv.size();
            CORBA::ORB_var clientORB = CORBA::ORB_init(argc, const_cast<char**>(clientArgv.data()));

            CORBA::Object_var obj = clientORB->string_to_object(ior);
            Basic_var model = Basic::_narrow(obj);

            model->ping();
            model->pong();
        });

        it("omniORB client calls omniORB server after it restarted throws OBJECT_NOT_EXIST / no match", [] {
            // CREATE SERVER
            array serverArgv = {"basic",
                                         // "-ORBtraceLevel", "25",
                                         "-ORBendPoint", "giop:tcp:127.0.0.1:9002"};
            int argc = serverArgv.size();
            CORBA::ORB_var serverORB = CORBA::ORB_init(argc, const_cast<char**>(serverArgv.data()));

            CORBA::Object_var serverObjPOA = serverORB->resolve_initial_references("RootPOA");
            PortableServer::POA_var serverRootPOA = PortableServer::POA::_narrow(serverObjPOA);
            PortableServer::POAManager_var serverPOAManager = serverRootPOA->the_POAManager();
            serverPOAManager->activate();

            auto impl = new Basic_impl();
            serverRootPOA->activate_object(impl);

            auto ior =
                "IOR:"
                "010000000e00000049444c3a42617369633a312e30000000010000000000000060000000010102000a0000003132372e302e302e31002a230e000000fe8eec5b6700000b3f0000"
                "00000000000200000000000000080000000100000000545441010000001c00000001000000010001000100000001000105090101000100000009010100";

            // OMNI CLIENT
            array clientArgv = {"basic",
                                         // "-ORBtraceLevel", "25",
                                         "-ORBendPoint", "giop:tcp:0.0.0.0:9001"};
            argc = clientArgv.size();
            CORBA::ORB_var clientORB = CORBA::ORB_init(argc, const_cast<char**>(clientArgv.data()));

            CORBA::Object_var obj = clientORB->string_to_object(ior);
            Basic_var model = Basic::_narrow(obj);

            unsigned long minor = 0;

            try {
                model->ping();
            } catch (CORBA::OBJECT_NOT_EXIST &ex) {
                minor = ex.minor();
            }
            expect(minor).to.equal(omni::OBJECT_NOT_EXIST_NoMatch);

            minor = 0;
            try {
                model->pong();
            } catch (CORBA::OBJECT_NOT_EXIST &ex) {
                minor = ex.minor();
            }
            expect(minor).to.equal(omni::OBJECT_NOT_EXIST_NoMatch);
        });

        it("calling set after it disappeared throws CORBA::TRANSIENT / connect failed", [] {
            array argv = {  "basic",
                            // "-ORBtraceLevel", "25",
                            "-ORBendPoint", "giop:tcp:0.0.0.0:9001"};
            int argc = argv.size();
            CORBA::ORB_var orb = CORBA::ORB_init(argc, const_cast<char**>(argv.data()));

            auto ior =
                "IOR:"
                "010000000e00000049444c3a42617369633a312e30000000010000000000000060000000010102000a0000003132372e302e302e310028230e000000febe79556700002ab20000"
                "00000000000200000000000000080000000100000000545441010000001c00000001000000010001000100000001000105090101000100000009010100";

            CORBA::Object_var obj = orb->string_to_object(ior);
            Basic_var model = Basic::_narrow(obj);

            unsigned long minor = 0;

            try {
                model->ping();
            } catch (CORBA::TRANSIENT &ex) {
                minor = ex.minor();
            }
            expect(minor).to.equal(omni::TRANSIENT_ConnectFailed);

            try {
                model->pong();
            } catch (CORBA::TRANSIENT &ex) {
                minor = ex.minor();
            }
            expect(minor).to.equal(omni::TRANSIENT_ConnectFailed);
        });

        it("omniORB client calls omniORB a 2nd time after it shut down throws CORBA::BAD_INV_ORDER / ORB has shut down", [] {
            // CREATE SERVER
            array serverArgv = {"basic",
                                // "-ORBtraceLevel", "25",
                                "-ORBendPoint", "giop:tcp:127.0.0.1:9002"};
            int argc = serverArgv.size();
            CORBA::ORB_var serverORB = CORBA::ORB_init(argc, const_cast<char**>(serverArgv.data()));

            CORBA::Object_var serverObjPOA = serverORB->resolve_initial_references("RootPOA");
            PortableServer::POA_var serverRootPOA = PortableServer::POA::_narrow(serverObjPOA);
            PortableServer::POAManager_var serverPOAManager = serverRootPOA->the_POAManager();
            serverPOAManager->activate();

            auto impl = new Basic_impl();
            serverRootPOA->activate_object(impl);

            CORBA::String_var ior = serverORB->object_to_string(serverRootPOA->servant_to_reference(impl));

            // println("{}", ior.operator char *());

            // OMNI CLIENT
            array clientArgv = {"basic",
                                // "-ORBtraceLevel", "25",
                                "-ORBendPoint", "giop:tcp:0.0.0.0:9001"};
            argc = clientArgv.size();
            CORBA::ORB_var clientORB = CORBA::ORB_init(argc, const_cast<char**>(clientArgv.data()));

            CORBA::Object_var obj = clientORB->string_to_object(ior);
            Basic_var model = Basic::_narrow(obj);

            model->ping();

            serverORB->destroy();

            unsigned long minor = 0;

            try {
                model->ping();
            } catch (CORBA::BAD_INV_ORDER &ex) {
                minor = ex.minor();
            }
            expect(minor).to.equal(omni::BAD_INV_ORDER_ORBHasShutdown);

            minor = 0;
            try {
                model->pong();
            } catch (CORBA::BAD_INV_ORDER &ex) {
                minor = ex.minor();
            }
            expect(minor).to.equal(omni::BAD_INV_ORDER_ORBHasShutdown);
        });
    });
});

void Basic_impl::ping() { 
    // println("GOT PINGED");
}
void Basic_impl::pong() {
    // println("GOT PONGED"); 
}