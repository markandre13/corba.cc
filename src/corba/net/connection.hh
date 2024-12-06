#pragma once

#include <atomic>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "../blob.hh"
#include "../coroutine.hh"
#include "util/socket.hh"

namespace CORBA {

class ORB;
class Stub;
class GIOPDecoder;

namespace detail {

class Protocol;

enum class ConnectionState {
    /**
     * The connection is not established nor is there a need for it to be established.
     */
    IDLE,
    /**
     * The connection needs to be establish but attempt to connect to the remote host has been made yet.
     */
    PENDING,
    /**
     * An attempt to connect to the remote host is in progress.
     */
    INPROGRESS,
    /**
     * A connection to the remote host has been established.
     */
    ESTABLISHED
};

class Connection {
        friend class CORBA::ORB;

        /**
         * for suspending coroutines via co_await interlock.suspend(requestId)
         */
        interlock<uint32_t, GIOPDecoder *> interlock;

        /**
         * counter to create new outgoing request ids
         */
        std::atomic_uint32_t requestId = 0;

    public:
        Protocol *protocol = nullptr;
        HostAndPort remote;

        Connection(Protocol *protocol, const char *host, uint16_t port) : protocol(protocol), remote(HostAndPort{host, port}) {}
        virtual ~Connection();

        ConnectionState state = ConnectionState::IDLE;

        std::map<blob, Stub *> stubsById;

        std::mutex send_mutex;
        // bi-directional service context needs only to be send once
        bool didSendBiDirIIOP = false;

        std::string str() const;
        virtual void up() = 0;
        virtual void send(std::unique_ptr<std::vector<char>> &&) = 0;
};

// FIXME: actually, we do not need the temporary ports and ip's:
// * if it's a server, all listen(host, port) combinations apply
// * if it's a client, it should also listen
// * if it's a client and it can not listen because of NAT, the client's host:port is send via IIOP
class ConnectionPool {
        // FIXME: this needs to be a map, set
        // std::set<shared_ptr<TcpConnection>, decltype(cmp)> connections;
        std::set<std::shared_ptr<Connection>> connections;

    public:
        inline void insert(std::shared_ptr<Connection> conn) { connections.insert(conn); }
        inline void erase(std::shared_ptr<Connection> conn) { connections.erase(conn); }
        inline void clear() { connections.clear(); }
        inline size_t size() const { return connections.size(); }
        Connection *find(const char *host, uint16_t port) const;
        void print() const;
};

}  // namespace detail
}  // namespace CORBA
