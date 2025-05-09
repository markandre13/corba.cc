#pragma once

#include "protocol.hh"
#include "../connection.hh"
#include "../stream2packet.hh"

#include <memory>
#include <vector>
#include <list>

namespace CORBA {

class ORB;
class Stub;
class GIOPDecoder;

namespace detail {

class TcpConnection : public Connection {
        // file descriptor handling
        int fd = -1;
        ev_io read_watcher;
        ev_io write_watcher;
        ev_timer timer_watcher;
        bool timer_active:1 = false;
        static void libev_read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
        static void libev_write_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
        static void libev_timer_cb(struct ev_loop *loop, struct ev_timer *watcher, int revents);

        // stream to packet
        IIOPStream2Packet stream2packet;

        // packet to stream
        std::list<std::unique_ptr<std::vector<char>>> sendBuffer;
        ssize_t bytesSend = 0;

    public:
        TcpConnection(Protocol *protocol, const char *host, uint16_t port);
        ~TcpConnection();

        void accept(int fd);
        void canWrite();
        void canRead();
        void timer();

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
        void startTimer();
        void stopTimer();
};

}  // namespace detail
}  // namespace CORBA
