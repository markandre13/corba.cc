#include "connection.hh"

#include <arpa/inet.h>
#include <unistd.h>

#include <print>

#include "../../exception.hh"
#include "../../orb.hh"

using namespace std;

namespace CORBA {
namespace detail {

static string prefix(WsConnection *conn) {
    string result;
    if (conn->protocol && conn->protocol->orb && conn->protocol->orb->logname) {
        result += format("ORB({}): TCP({}): ", conn->protocol->orb->logname, conn->str());
    }
    return result;
}

WsConnection::WsConnection(Protocol *protocol, const char *host, uint16_t port, WsConnectionState initialState)
    : Connection(protocol, host, port), wsstate(initialState) {}

WsConnection::~WsConnection() {
    // stopTimer();
    // if (fd != -1) {
    //     auto loc = getLocalName(fd);
    //     auto peer = getPeerName(fd);
    //     println("WsConnection::~WsConnection(): {} -> {}", loc.str(), peer.str());
    //     stopWriteHandler();
    //     stopReadHandler();
    //     ::close(fd);
    // }
}

void WsConnection::send(unique_ptr<vector<char>> &&buffer) {
    // println("{}WsConnection::send(): {} bytes", prefix(this), buffer->size());
    // sendBuffer.push_back(move(buffer));
    // switch (state) {
    //     case ConnectionState::IDLE:
    //         up();
    //         break;
    //     case ConnectionState::ESTABLISHED:
    //         startWriteHandler();
    //         break;
    // }
}

void WsConnection::recv(void *buffer, size_t nbytes) {
    // println("{}WsConnection::recv(): {} bytes", prefix(this), nbytes);
    // if (protocol && protocol->orb) {
    //     protocol->orb->socketRcvd(this, buffer, nbytes);
    // }
    // if (receiver) {
    //     receiver(buffer, nbytes);
    // }
}

void WsConnection::accept(int client) {
    // if (fd != -1) {
    //     throw runtime_error("WsConnection::accept(int fd): fd is already set");
    // }

    // fd = client;
    // ev_io_init(&read_watcher, libev_read_cb, fd, EV_READ);
    // ev_io_init(&write_watcher, libev_write_cb, fd, EV_WRITE);

    // startReadHandler();
    // state = ConnectionState::ESTABLISHED;
}

void WsConnection::canWrite() {
    // println("{}WsConnection::canWrite(): sendbuffer size = {}, fd = {}", prefix(this), sendBuffer.size(), fd);
    // stopWriteHandler();

    // // https://stackoverflow.com/questions/17769964/linux-sockets-non-blocking-connect
    // // https://cr.yp.to/docs/connect.html
    // // Once the system signals the socket as writable, first call getpeername() to see if it connected or not.
    // // If that call succeeded, the socket connected and you can start using it. If that call fails with ENOTCONN,
    // // the connection failed. To find out why it failed, try to read one byte from the socket read(fd, &ch, 1),
    // // which will fail as well but the error you get is the error you would have gotten from connect() if it wasn't
    // // non-blocking.
    // if (state == ConnectionState::INPROGRESS) {
    //     struct sockaddr_storage addr;
    //     socklen_t len = sizeof(addr);
    //     if (getpeername(fd, (sockaddr *)&addr, &len) != 0) {
    //         if (errno == EINVAL) {
    //             println("{}WsConnection::canWrite(): INPROGRESS -> IDLE (not connected)", prefix(this));
    //         } else {
    //             println("{}WsConnection::canWrite(): INPROGRESS -> IDLE ({} ({}))", prefix(this), strerror(errno), errno);
    //         }
    //         // TODO: when bidirectional or there are packets to be send, go to PENDING instead of IDLE
    //         state = ConnectionState::IDLE;
    //         close(fd);
    //         fd = -1;
    //         return;
    //     } else {
    //         // NOTE: this only means that we can write to the operating system's buffer, not that data
    //         //       will be send to the remote peer
    //         // println("{}WsConnection::canWrite(): INPROGRESS -> ESTABLISHED", prefix(this));
    //         // state = ConnectionState::ESTABLISHED;
    //     }
    // }

    // while (!sendBuffer.empty()) {
    //     auto data = sendBuffer.front()->data();
    //     auto nbytes = sendBuffer.front()->size();

    //     ssize_t n = ::send(fd, data + bytesSend, nbytes - bytesSend, 0);

    //     if (n >= 0) {
    //         println("{}WsConnection::canWrite(): sendbuffer size {}: send {} bytes at {} of out {}", prefix(this), sendBuffer.size(), n, bytesSend,
    //                 nbytes - bytesSend);
    //     } else {
    //         if (errno == EPIPE) {
    //             println("{}WsConnection::canWrite(): broken connection -> IDLE", prefix(this));
    //             // TODO: when bidirectional or there are packets to be send, go to PENDING instead of IDLE
    //             state = ConnectionState::IDLE;
    //             close(fd);
    //             fd = -1;
    //             return;
    //         } else if (errno != EAGAIN) {
    //             println("{}WsConnection::canWrite(): sendbuffer size {}: error: {} ({})", prefix(this), sendBuffer.size(), strerror(errno), errno);
    //         } else {
    //             println("{}WsConnection::canWrite(): sendbuffer size {}: wait", prefix(this), sendBuffer.size());
    //         }
    //         break;
    //     }

    //     if (n != nbytes - bytesSend) {
    //         // println("************************************************** incomplete send");
    //         bytesSend += n;
    //         break;
    //     } else {
    //         sendBuffer.pop_front();
    //         bytesSend = 0;
    //     }
    // }

    // if (!sendBuffer.empty()) {
    //     println("{}WsConnection::canWrite(): sendbuffer size {}: register write handler to send more", prefix(this), sendBuffer.size());
    //     startWriteHandler();
    // } else {
    //     println("{}WsConnection::canWrite(): sendbuffer size {}: nothing more to send", prefix(this), sendBuffer.size());
    // }
}

void WsConnection::canRead() {
    // println("{}WsConnection::canRead()", prefix(this));
    // ssize_t nbytes = ::recv(fd, stream2packet.buffer(), stream2packet.length(), 0);
    // if (nbytes < 0) {
    //     if (errno == EAGAIN) {
    //         println("{}WsConnection::canRead(): {} (EGAIN)", prefix(this), strerror(errno));
    //         return;
    //     }
    //     if (errno == EBADF) {
    //         println("{}WsConnection::canRead(): failed to connect to peer", prefix(this));
    //     } else {
    //         println("{}WsConnection::canRead(): {} ({})", prefix(this), strerror(errno), errno);
    //     }
    //     stopReadHandler();
    //     ::close(fd);
    //     fd = -1;
    //     // TODO: if there packets to be send, switch to pending
    //     // TODO: have one method to switch the state and perform the needed actions (e.g. handle timers)?
    //     state = ConnectionState::IDLE;
    //     while(!interlock.empty()) {
    //         interlock.resume(interlock.begin()->first, make_exception_ptr(TRANSIENT(0, CORBA::CompletionStatus::NO)));
    //     }
    //     return;
    // }
    // println("{}WsConnection::canRead(): state = {}", prefix(this), std::to_underlying(state));
    // if (state == ConnectionState::INPROGRESS) {
    //     state = ConnectionState::ESTABLISHED;
    //     stopTimer();
    // }
    // println("{}recv'd {} bytes", prefix(this), nbytes);
    // if (nbytes > 0) {
    //     stream2packet.received(nbytes);
    //     while(true) {
    //         auto msg = stream2packet.message();
    //         if (msg.empty()) {
    //             break;
    //         }
    //         recv(msg.data(), msg.size());
    //     }
    // }
}

void WsConnection::up() {
    // if (fd >= 0) {
    //     return;
    // }

    // println("{}WsConnection::up(): -> {}", prefix(this), str());

    // fd = connect_to(remote.host.c_str(), remote.port);
    // if (fd < 0) {
    //     println("{}WsConnection::up(): -> PENDING", prefix(this));
    //     state = ConnectionState::PENDING;
    //     throw runtime_error(format("WsConnection()::up(): {}: {}", remote.str(), strerror(errno)));
    // }

    // // TODO: move these into the constructor?
    // ev_io_init(&read_watcher, libev_read_cb, fd, EV_READ);
    // ev_io_init(&write_watcher, libev_write_cb, fd, EV_WRITE);
    // startTimer();

    // if (errno == EINPROGRESS) {
    //     println("{}WsConnection::up(): -> INPROGRESS", prefix(this));
    //     state = ConnectionState::INPROGRESS;
    //     startWriteHandler();
    // } else {
    //     println("{}WsConnection::up(): -> ESTABLISHED", prefix(this));
    //     state = ConnectionState::ESTABLISHED;
    // }
    // startReadHandler();
}

void WsConnection::libev_read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    // auto connection = reinterpret_cast<WsConnection *>(reinterpret_cast<char*>(watcher) - offsetof(WsConnection, read_watcher));
    // connection->canRead();
}

void WsConnection::libev_write_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    // auto connection = reinterpret_cast<WsConnection *>(reinterpret_cast<char*>(watcher) - offsetof(WsConnection, write_watcher));
    // connection->canWrite();
}

void WsConnection::libev_timer_cb(struct ev_loop *loop, struct ev_timer *watcher, int revents) {
    // auto connection = reinterpret_cast<WsConnection *>(reinterpret_cast<char*>(watcher) - offsetof(WsConnection, timer_watcher));
    // connection->timer();
}

void WsConnection::stopReadHandler() {
    // if (!ev_is_active(&read_watcher)) {
    //     return;
    // }
    // println("{}stop read handler", prefix(this));
    // ev_io_stop(protocol->loop, &read_watcher);
}

void WsConnection::stopWriteHandler() {
    // if (!ev_is_active(&write_watcher)) {
    //     return;
    // }
    // ev_io_stop(protocol->loop, &write_watcher);
}

void WsConnection::startReadHandler() {
    // if (ev_is_active(&read_watcher)) {
    //     return;
    // }
    // println("{}start read handler", prefix(this));
    // ev_io_start(protocol->loop, &read_watcher);
}

void WsConnection::startWriteHandler() {
    // if (ev_is_active(&write_watcher)) {
    //     return;
    // }
    // ev_io_start(protocol->loop, &write_watcher);
}

void WsConnection::startTimer() {
    // if (timer_active) {
    //     return;
    // }
    // println("startTimer");
    // timer_active = true;
    // ev_timer_init(&timer_watcher, libev_timer_cb, 1, 0.);
    // ev_timer_start(protocol->loop, &timer_watcher);
}
void WsConnection::stopTimer() {
    // if (!timer_active) {
    //     return;
    // }
    // println("stopTimer");
    // timer_active = false;
    // ev_timer_stop(protocol->loop, &timer_watcher);
}
void WsConnection::timer() {
    // println("timer {}", std::to_underlying(state));
    // if (state == ConnectionState::INPROGRESS) {
    //     println("INPROGRESS -> TIMEOUT");
    //     stopReadHandler();
    //     ::close(fd);
    //     fd = -1;
    //     state = ConnectionState::IDLE;
    //     while(!interlock.empty()) {
    //         interlock.resume(interlock.begin()->first, make_exception_ptr(TIMEOUT(0, CORBA::CompletionStatus::NO)));
    //     }
    // }
}

void WsConnection::print() {
    // println(" -> {}", remote.str());
    // println("{}:{} -> {}:{}", protocol->localHost, protocol->localPort, localHost, localPort);
}

}  // namespace detail
}  // namespace CORBA
