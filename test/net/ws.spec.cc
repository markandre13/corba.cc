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
#include "../src/corba/corba.hh"
#include "../src/corba/net/ws/connection.hh"
#include "../src/corba/net/ws/protocol.hh"
#include "../src/corba/util/logger.hh"
#include "../util.hh"
#include "kaffeeklatsch.hh"

using namespace kaffeeklatsch;
using namespace std;
using CORBA::async;

class SysLogger : public LogDestination {
    protected:
        /**
         * \param option LOG_NOWAIT, LOG_ODELAY
         * \param facility LOG_AUTH, LOG_DAEMON, LOG_KERN, ..., LOG_USER
         */
        SysLogger(const char *id, int option, int facility) { openlog(id, option, facility); }
        virtual void log(int priority, const char *message) override { syslog(priority, "%s", message); }
};

struct LogEntry {
        int level;
        std::time_t time;
        std::string message;

        LogEntry(int level, std::time_t time, std::string message) : level(level), time(time), message(message) {}
};

class MemoryLogger : public LogDestination {
    public:
        vector<LogEntry> logs;
        inline void clear() { logs.clear(); }

    protected:
        virtual void log(int priority, const char *message) override { logs.push_back(LogEntry(priority, std::time({}), message)); }
};

std::shared_ptr<MemoryLogger> logger;

kaffeeklatsch_spec([] {
    beforeAll([&] {
        logger = make_shared<MemoryLogger>();
        Logger::setDestination(logger);
    });
    beforeEach([&] {
        logger->clear();
    });
    describe("log", [] {
        fit("log", [] {
            Logger::info("hello {} {}", 1, "you");
            expect(logger->logs.size()).is.equal(1);
            expect(logger->logs[0].message).is.equal("hello 1 you");  // TODO: timestamp, logger, etc. and do not place it into a string!!!
        });
    });
    describe("net", [] {
        describe("websocket", [] {
            it("bi-directional iiop connection", [] {
                struct ev_loop *loop = EV_DEFAULT;

                auto serverORB = make_shared<CORBA::ORB>("server");
                serverORB->debug = true;
                auto serverProto = new CORBA::detail::WsProtocol(loop);
                serverORB->registerProtocol(serverProto);
                serverProto->listen("127.0.0.1", 9003);

                auto backend = make_shared<Interface_impl>();
                serverORB->bind("Backend", backend);

                std::exception_ptr eptr;

                auto clientORB = make_shared<CORBA::ORB>("client");
                clientORB->debug = true;
                auto clientProto = new CORBA::detail::WsProtocol(loop);
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
            });
        });
    });
});