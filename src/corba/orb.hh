#pragma once

#include <functional>
#include <map>
#include <vector>
#include <memory>

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
        const char * logname = nullptr;

    protected:
        NamingContextExtImpl * namingService = nullptr;
        // std::map<std::string, Skeleton*> initialReferences; // name to
        std::map<blob, Skeleton *> servants;  // objectId to skeleton

        uint64_t servantIdCounter = 0;

        std::vector<detail::Protocol*> protocols;
    public:
        ORB(const char *logname = nullptr): logname(logname) {};
        ~ORB();
        
        detail::ConnectionPool connections;

        void registerProtocol(detail::Protocol *protocol);
        detail::Connection * getConnection(std::string host, uint16_t port);
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

        // register servant and create and assign a new objectKey
        blob_view registerServant(Skeleton *skeleton);
        // register servant with the given objectKey
        blob_view registerServant(Skeleton *skeleton, const std::string &objectKey);

        template <typename T>
        async<T> twowayCall(
            Stub *stub,
            const char *operation,
            std::function<void(GIOPEncoder &)> encode,
            std::function<T(GIOPDecoder &)> decode)
        {
            auto decoder = co_await _twowayCall(stub, operation, encode);
            co_return decode(*decoder);
        }

        // template <typename T>
        async<void> twowayCall(
            Stub *stub,
            const char *operation,
            std::function<void(GIOPEncoder &)> encode)
        {
            co_await _twowayCall(stub, operation, encode);
            co_return;
        }

        void onewayCall(
            Stub *stub,
            const char *operation,
            std::function<void(GIOPEncoder &)> encode);

        //
        // NameService
        //
        void bind(const std::string &id, std::shared_ptr<CORBA::Skeleton> const obj);

    protected:
        async<GIOPDecoder*> _twowayCall(Stub *stub, const char *operation, std::function<void(GIOPEncoder &)> encode);
};

}  // namespace CORBA
