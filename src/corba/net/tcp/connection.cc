#include "connection.hh"
#include "../../exception.hh"
#include "../../orb.hh"

#include <unistd.h>
#include <arpa/inet.h>
#include <print>

using namespace std;

namespace CORBA {
namespace detail {

static string prefix(TcpConnection *conn) {
    string result;
    if (conn->protocol && conn->protocol->orb && conn->protocol->orb->logname) {
        result += format("ORB({}): TCP({}): ", conn->protocol->orb->logname, conn->str());
    }
    return result;
}

void TcpConnection::send(unique_ptr<vector<char>> &&buffer) {
    println("{}TcpConnection::send(): {} bytes", prefix(this), buffer->size());
    sendBuffer.push_back(move(buffer));
    switch (state) {
        case ConnectionState::IDLE:
            up();
            break;
        case ConnectionState::ESTABLISHED:
            startWriteHandler();
            break;
    }
}

void TcpConnection::recv(void *buffer, size_t nbytes) {
    println("{}TcpConnection::recv(): {} bytes", prefix(this), nbytes);
    if (protocol && protocol->orb) {
        protocol->orb->socketRcvd(this, buffer, nbytes);
    }
    if (receiver) {
        receiver(buffer, nbytes);
    }
}

void TcpConnection::accept(int client) {
    if (fd != -1) {
        throw runtime_error("TcpConnection::accept(int fd): fd is already set");
    }

    fd = client;
    ev_io_init(&read_watcher, libev_read_cb, fd, EV_READ);
    ev_io_init(&write_watcher, libev_write_cb, fd, EV_WRITE);

    startReadHandler();
    state = ConnectionState::ESTABLISHED;
}

TcpConnection::~TcpConnection() {
    stopTimer();
    if (fd != -1) {
        auto loc = getLocalName(fd);
        auto peer = getPeerName(fd);
        println("TcpConnection::~TcpConnection(): {} -> {}", loc.str(), peer.str());
        stopWriteHandler();
        stopReadHandler();
        ::close(fd);
    }
}

void TcpConnection::canWrite() {
    println("{}TcpConnection::canWrite(): sendbuffer size = {}, fd = {}", prefix(this), sendBuffer.size(), fd);
    stopWriteHandler();

    // https://stackoverflow.com/questions/17769964/linux-sockets-non-blocking-connect
    // https://cr.yp.to/docs/connect.html
    // Once the system signals the socket as writable, first call getpeername() to see if it connected or not.
    // If that call succeeded, the socket connected and you can start using it. If that call fails with ENOTCONN,
    // the connection failed. To find out why it failed, try to read one byte from the socket read(fd, &ch, 1),
    // which will fail as well but the error you get is the error you would have gotten from connect() if it wasn't
    // non-blocking.
    if (state == ConnectionState::INPROGRESS) {
        struct sockaddr_storage addr;
        socklen_t len = sizeof(addr);
        if (getpeername(fd, (sockaddr *)&addr, &len) != 0) {
            if (errno == EINVAL) {
                println("{}TcpConnection::canWrite(): INPROGRESS -> IDLE (not connected)", prefix(this));
            } else {
                println("{}TcpConnection::canWrite(): INPROGRESS -> IDLE ({} ({}))", prefix(this), strerror(errno), errno);
            }
            // TODO: when bidirectional or there are packets to be send, go to PENDING instead of IDLE
            state = ConnectionState::IDLE;
            close(fd);
            fd = -1;
            return;
        } else {
            // NOTE: this only means that we can write to the operating system's buffer, not that data
            //       will be send to the remote peer
            // println("{}TcpConnection::canWrite(): INPROGRESS -> ESTABLISHED", prefix(this));
            // state = ConnectionState::ESTABLISHED;
        }
    }

    while (!sendBuffer.empty()) {
        auto data = sendBuffer.front()->data();
        auto nbytes = sendBuffer.front()->size();

        ssize_t n = ::send(fd, data + bytesSend, nbytes - bytesSend, 0);

        if (n >= 0) {
            println("{}TcpConnection::canWrite(): sendbuffer size {}: send {} bytes at {} of out {}", prefix(this), sendBuffer.size(), n, bytesSend,
                    nbytes - bytesSend);
        } else {
            if (errno == EPIPE) {
                println("{}TcpConnection::canWrite(): broken connection -> IDLE", prefix(this));
                // TODO: when bidirectional or there are packets to be send, go to PENDING instead of IDLE
                state = ConnectionState::IDLE;
                close(fd);
                fd = -1;
                return;
            } else if (errno != EAGAIN) {
                println("{}TcpConnection::canWrite(): sendbuffer size {}: error: {} ({})", prefix(this), sendBuffer.size(), strerror(errno), errno);
            } else {
                println("{}TcpConnection::canWrite(): sendbuffer size {}: wait", prefix(this), sendBuffer.size());
            }
            break;
        }

        if (n != nbytes - bytesSend) {
            // println("************************************************** incomplete send");
            bytesSend += n;
            break;
        } else {
            sendBuffer.pop_front();
            bytesSend = 0;
        }
    }

    if (!sendBuffer.empty()) {
        println("{}TcpConnection::canWrite(): sendbuffer size {}: register write handler to send more", prefix(this), sendBuffer.size());
        startWriteHandler();
    } else {
        println("{}TcpConnection::canWrite(): sendbuffer size {}: nothing more to send", prefix(this), sendBuffer.size());
    }
}

void TcpConnection::canRead() {
    println("{}TcpConnection::canRead()", prefix(this));
    ssize_t nbytes = ::recv(fd, stream2packet.buffer(), stream2packet.length(), 0);
    if (nbytes < 0) {
        if (errno == EAGAIN) {
            println("{}TcpConnection::canRead(): {} (EGAIN)", prefix(this), strerror(errno));
            return;
        }
        if (errno == EBADF) {
            println("{}TcpConnection::canRead(): failed to connect to peer", prefix(this));
        } else {
            println("{}TcpConnection::canRead(): {} ({})", prefix(this), strerror(errno), errno);
        }
        stopReadHandler();
        ::close(fd);
        fd = -1;
        // TODO: if there packets to be send, switch to pending
        // TODO: have one method to switch the state and perform the needed actions (e.g. handle timers)?
        state = ConnectionState::IDLE;
        while(!interlock.empty()) {
            interlock.resume(interlock.begin()->first, make_exception_ptr(TRANSIENT(0, CORBA::CompletionStatus::NO)));
        }
        return;
    }
    println("{}TcpConnection::canRead(): state = {}", prefix(this), std::to_underlying(state));
    if (state == ConnectionState::INPROGRESS) {
        state = ConnectionState::ESTABLISHED;
        stopTimer();
    }
    println("{}recv'd {} bytes", prefix(this), nbytes);
    if (nbytes > 0) {
        stream2packet.received(nbytes);
        while(true) {
            auto msg = stream2packet.message();
            if (msg.empty()) {
                break;
            }
            recv(msg.data(), msg.size());
        }
    }
}

void TcpConnection::up() {
    if (fd >= 0) {
        return;
    }

    println("{}TcpConnection::up(): -> {}", prefix(this), str());

    fd = connect_to(remote.host.c_str(), remote.port);
    if (fd < 0) {
        println("{}TcpConnection::up(): -> PENDING", prefix(this));
        state = ConnectionState::PENDING;
        throw runtime_error(format("TcpConnection()::up(): {}: {}", remote.str(), strerror(errno)));
    }

    // TODO: move these into the constructor?
    ev_io_init(&read_watcher, libev_read_cb, fd, EV_READ);
    ev_io_init(&write_watcher, libev_write_cb, fd, EV_WRITE);
    startTimer();

    if (errno == EINPROGRESS) {
        println("{}TcpConnection::up(): -> INPROGRESS", prefix(this));
        state = ConnectionState::INPROGRESS;
        startWriteHandler();
    } else {
        println("{}TcpConnection::up(): -> ESTABLISHED", prefix(this));
        state = ConnectionState::ESTABLISHED;
    }
    startReadHandler();
}

TcpConnection::TcpConnection(Protocol *protocol, const char *host, uint16_t port) : Connection(protocol, host, port) {
}

void TcpConnection::libev_read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    auto connection = reinterpret_cast<TcpConnection *>(reinterpret_cast<char*>(watcher) - offsetof(TcpConnection, read_watcher));
    connection->canRead();
}

void TcpConnection::libev_write_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    auto connection = reinterpret_cast<TcpConnection *>(reinterpret_cast<char*>(watcher) - offsetof(TcpConnection, write_watcher));
    connection->canWrite();
}

void TcpConnection::libev_timer_cb(struct ev_loop *loop, struct ev_timer *watcher, int revents) {
    auto connection = reinterpret_cast<TcpConnection *>(reinterpret_cast<char*>(watcher) - offsetof(TcpConnection, timer_watcher));
    connection->timer();
}

void TcpConnection::stopReadHandler() {
    if (!ev_is_active(&read_watcher)) {
        return;
    }
    println("{}stop read handler", prefix(this));
    ev_io_stop(protocol->loop, &read_watcher);
}

void TcpConnection::stopWriteHandler() {
    if (!ev_is_active(&write_watcher)) {
        return;
    }
    ev_io_stop(protocol->loop, &write_watcher);
}

void TcpConnection::startReadHandler() {
    if (ev_is_active(&read_watcher)) {
        return;
    }
    println("{}start read handler", prefix(this));
    ev_io_start(protocol->loop, &read_watcher);
}

void TcpConnection::startWriteHandler() {
    if (ev_is_active(&write_watcher)) {
        return;
    }
    ev_io_start(protocol->loop, &write_watcher);
}

void TcpConnection::startTimer() {
    if (timer_active) {
        return;
    }
    println("startTimer");
    timer_active = true;
    ev_timer_init(&timer_watcher, libev_timer_cb, 1, 0.);
    ev_timer_start(protocol->loop, &timer_watcher);
}
void TcpConnection::stopTimer() {
    if (!timer_active) {
        return;
    }
    println("stopTimer");
    timer_active = false;
    ev_timer_stop(protocol->loop, &timer_watcher);
}
void TcpConnection::timer() {
    println("timer {}", std::to_underlying(state));
    if (state == ConnectionState::INPROGRESS) {
        println("INPROGRESS -> TIMEOUT");
        stopReadHandler();
        ::close(fd);
        fd = -1;
        state = ConnectionState::IDLE;
        while(!interlock.empty()) {
            interlock.resume(interlock.begin()->first, make_exception_ptr(TIMEOUT(0, CORBA::CompletionStatus::NO)));
        }
    }
}

void TcpConnection::print() {
    println(" -> {}", remote.str());
    // println("{}:{} -> {}:{}", protocol->localHost, protocol->localPort, localHost, localPort);
}

}  // namespace detail
}  // namespace CORBA
