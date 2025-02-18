#pragma once

#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "coroutine.hh"
#include "giop.hh"
#include "net/connection.hh"

namespace CORBA {

class Skeleton;
class Stub;
class NamingContextExtImpl;
class ORB;

namespace detail {
class Protocol;
}

/**
 * Usually one uses try/catch to handle exceptions. But some exceptions like
 *
 * TRANSIENT: failed to connect to peer
 * TIMEOUT: timeout for response exceeded
 * COMM_FAILURE: connection closed
 *
 * may occur everywhere, hence one can register additional exception handlers
 * which handle event in case the application did not catch them.
 *
 * This is not part of the CORBA standard but borrowed from OmniORB.
 */
void installSystemExceptionHandler(std::shared_ptr<CORBA::Object> object, std::function<void()> handler);
// installTimeoutExceptionHandler
// installTransientExceptionHandler
// installCommFailureExceptionHandler

class ORB : public std::enable_shared_from_this<ORB> {
    public:
        bool debug = false;
        void dump();
        const char *logname = nullptr;

    protected:
        std::shared_ptr<NamingContextExtImpl> namingService;
        // std::map<std::string, Skeleton*> initialReferences; // name to
    public:
        /**
         * objectId to skeleton/implementation
         */
        std::map<blob, std::shared_ptr<Skeleton>> servants;

        uint64_t servantIdCounter = 0;

        std::vector<detail::Protocol *> protocols;

    public:
        ORB(const char *logname = nullptr) : logname(logname) {};
        ~ORB();
        void shutdown();

        detail::ConnectionPool connections;

        void registerProtocol(detail::Protocol *protocol);
        std::shared_ptr<detail::Connection> getConnection(std::string host, uint16_t port);
        // void addConnection(detail::Connection *connection) { connections.push_back(connection); }
        void socketRcvd(detail::Connection *connection, const void *buffer, size_t size);
        void close(detail::Connection *connection);

        /**
         * Returns an object for the provided CORBA URI.
         *
         * "corbaname::127.0.0.1:9003#Backend" will contact the CORBA nameservice at
         * 127.0.0.1:9003 and request an object registered by the name of "Backend".
         */
        async<std::shared_ptr<Object>> stringToObject(const std::string &iorString);

        // this is usually part of the POA but corba.cc doesn't have one (yet?)
        void activate_object(std::shared_ptr<Skeleton> servant);
        void activate_object_with_id(const std::string &objectKey, std::shared_ptr<Skeleton> servant);

        /**
         * call the peer and wait for a response
         *
         * \param stub stub which invoked the operation
         * \param operation name of the operation (aka. function/method name)
         * \param encode callback encoding the outgoing arguments
         * \param decode callback decoding the incoming arguments
         */
        template <typename T>
        async<T> twowayCall(Stub *stub, const char *operation, std::function<void(GIOPEncoder &)> encode, std::function<T(GIOPDecoder &)> decode) {
            auto decoder = co_await _twowayCall(stub, operation, encode);
            co_return decode(*decoder);
        }

        async<void> twowayCall(Stub *stub, const char *operation, std::function<void(GIOPEncoder &)> encode) {
            co_await _twowayCall(stub, operation, encode);
            co_return;
        }

        /**
         * call the peer without waiting for a response (oneway)
         */
        void onewayCall(Stub *stub, const char *operation, std::function<void(GIOPEncoder &)> encode);

        //
        // NameService
        //

        /**
         * register obj as id in the internat nameserver
         *
         * \param id
         * \param obj
         */
        void bind(const std::string &id, std::shared_ptr<CORBA::Skeleton> const obj);

        std::shared_ptr<CORBA::Skeleton> _narrow_servant(CORBA::IOR *ref);

    protected:
        async<GIOPDecoder *> _twowayCall(Stub *stub, const char *operation, std::function<void(GIOPEncoder &)> encode);
};

}  // namespace CORBA
