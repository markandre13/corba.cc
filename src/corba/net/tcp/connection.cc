#include "connection.hh"
#include "../../orb.hh"

#include <unistd.h>
#include <arpa/inet.h>
#include <print>

using namespace std;

namespace CORBA {
namespace detail {

static void libev_read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
static void libev_write_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);

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
            println("{}TcpConnection::canWrite(): INPROGRESS -> ESTABLISHED", prefix(this));
            state = ConnectionState::ESTABLISHED;
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
    char buffer[8192];  // TODO: put GIOPStream2Packets in here!!!
    ssize_t nbytes = ::recv(fd, buffer, sizeof(buffer), 0);
    if (nbytes < 0) {
        if (errno == EAGAIN) {
            println("{}TcpConnection::canRead(): {} (EGAIN)", prefix(this), strerror(errno));
            return;
        }
        println("{}TcpConnection::canRead(): {} ({})", prefix(this), strerror(errno), errno);
        stopReadHandler();
        ::close(fd);
        fd = -1;
        // TODO: if there packets to be send, switch to pending
        // TODO: have one method to switch the state and perform the needed actions (e.g. handle timers)?
        state = ConnectionState::IDLE;
        return;
    }
    println("{}TcpConnection::canRead(): state = {}", prefix(this), std::to_underlying(state));
    if (state == ConnectionState::INPROGRESS) {
        state = ConnectionState::ESTABLISHED;
    }
    println("{}recv'd {} bytes", prefix(this), nbytes);
    if (nbytes > 0) {
        recv(buffer, nbytes);
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

    ev_io_init(&read_watcher, libev_read_cb, fd, EV_READ);
    ev_io_init(&write_watcher, libev_write_cb, fd, EV_WRITE);

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

void libev_read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    auto connection = reinterpret_cast<TcpConnection *>(reinterpret_cast<char*>(watcher) - offsetof(TcpConnection, read_watcher));
    connection->canRead();
}

void libev_write_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    auto connection = reinterpret_cast<TcpConnection *>(reinterpret_cast<char*>(watcher) - offsetof(TcpConnection, write_watcher));
    connection->canWrite();
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

void TcpConnection::print() {
    println(" -> {}", remote.str());
    // println("{}:{} -> {}:{}", protocol->localHost, protocol->localPort, localHost, localPort);
}

}  // namespace detail
}  // namespace CORBA
