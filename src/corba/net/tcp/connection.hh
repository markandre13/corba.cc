#pragma once

#include "protocol.hh"
#include "../connection.hh"

#include <memory>
#include <vector>
#include <list>

namespace CORBA {

class ORB;
class Stub;
class GIOPDecoder;

namespace detail {

// class read_handler_t;
// class write_handler_t;

class TcpConnection;

struct read_handler_t {
        ev_io watcher;
        TcpConnection *connection;
};

struct write_handler_t {
        ev_io watcher;
        TcpConnection *connection;
};

class TcpConnection : public Connection {
        int fd = -1;
        std::unique_ptr<read_handler_t> readHandler;
        std::unique_ptr<write_handler_t> writeHandler;

        std::list<std::unique_ptr<std::vector<char>>> sendBuffer;
        ssize_t bytesSend = 0;

    public:
        TcpConnection(Protocol *protocol, const char *host, uint16_t port) : Connection(protocol, host, port) {}
        ~TcpConnection();

        void accept(int fd);
        void canWrite();
        void canRead();

        std::function<void(void *buffer, size_t nbyte)> receiver;

        void up() override;
        void send(std::unique_ptr<std::vector<char>> &&) override;
        void recv(void *buffer, size_t nbyte);

        void print();
        inline int getFD() { return this->fd; }

    private:
        void startReadHandler();
        void stopReadHandler();
        void startWriteHandler();
        void stopWriteHandler();
};

}  // namespace detail
}  // namespace CORBA
