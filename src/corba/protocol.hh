#pragma once

#include <map>
#include <string>

#include "blob.hh"
#include "coroutine.hh"

namespace CORBA {

class ORB;
class Stub;
class GIOPDecoder;

namespace detail {

class Connection;

/**
 * Protocol provides an interface to the ORB for various network protocols, eg.
 *
 * orb.registerProtocol(new TcpProtocol(orb))
 * orb.registerProtocol(new WebSocketProtocol(orb))
 */
class Protocol {
    public:
        // hm... do we return something or do we call ORB?
        virtual async<detail::Connection*> connect(const ORB *orb, const std::string &hostname, uint16_t port) = 0;
        virtual async<> close() = 0;
};

class Connection {
        friend class CORBA::ORB;

        // ORB *orb;


        /**
         * for suspending coroutines via
         *   co_await interlock.suspend(requestId)
         *
         */
        interlock<uint32_t, GIOPDecoder *> interlock;

        // stubs may contain OID received via this connection
        // TODO: WeakMap? refcount tests
        // objectId to stub?
        std::map<blob, Stub *> stubsById;

        /**
         * counter to create new outgoing request ids
         */
        uint32_t requestId = 0;

    public:
        // bi-directional service context needs only to be send once
        bool didSendBiDirIIOP = false;

        Connection(uint32_t initialRequestId = 0) {}
        // replies to be send back over this connection
        // number: RequestId
        // WeakMap? refcount tests
        // pendingReplies = new Map<number, PromiseHandler>()

        // CSIv2 context tokens received by the client
        // BigInt: ContextId
        // initialContextTokens = new Map<BigInt, InitialContextToken>()
        virtual void addPeer(const std::string_view &hostname, uint16_t port) {}

        virtual const std::string &localAddress() const = 0;
        virtual uint16_t localPort() const = 0;
        virtual const std::string &remoteAddress() const = 0;
        virtual uint16_t remotePort() const = 0;

        virtual void close() = 0;
        virtual void send(void *buffer, size_t nbyte) = 0;
};

}  // namespace detail

}  // namespace CORBA
