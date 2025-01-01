#include "connection.hh"

#include <arpa/inet.h>
#include <unistd.h>

#include <print>

#include "../../exception.hh"
#include "../../orb.hh"
#include "../util/createAcceptKey.hh"

using namespace std;

namespace CORBA {
namespace detail {

static ssize_t wslay_send_callback(wslay_event_context_ptr ctx, const uint8_t *data, size_t len, int flags, void *user_data);
static ssize_t wslay_recv_callback(wslay_event_context_ptr ctx, uint8_t *data, size_t len, int flags, void *user_data);
static void wslay_msg_rcv_callback(wslay_event_context_ptr ctx, const struct wslay_event_on_msg_recv_arg *arg, void *user_data);
static int genmask_callback(wslay_event_context_ptr ctx, uint8_t *buf, size_t len, void *user_data);

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
    stopTimer();
    if (fd != -1) {
        auto loc = getLocalName(fd);
        auto peer = getPeerName(fd);
        println("WsConnection::~WsConnection(): {} -> {}", loc.str(), peer.str());
        stopWriteHandler();
        stopReadHandler();
        ::close(fd);
    }
}

void WsConnection::send(unique_ptr<vector<char>> &&buffer) {
    println("{}WsConnection::send(): {} bytes", prefix(this), buffer->size());
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

void WsConnection::recv(void *buffer, size_t nbytes) {
    println("{}WsConnection::recv(): {} bytes", prefix(this), nbytes);
    // if (protocol && protocol->orb) {
    //     protocol->orb->socketRcvd(this, buffer, nbytes);
    // }
    // if (receiver) {
    //     receiver(buffer, nbytes);
    // }
}

void WsConnection::accept(int client) {
    if (fd != -1) {
        throw runtime_error("WsConnection::accept(int fd): fd is already set");
    }

    fd = client;
    ev_io_init(&read_watcher, libev_read_cb, fd, EV_READ);
    ev_io_init(&write_watcher, libev_write_cb, fd, EV_WRITE);

    startReadHandler();
    state = ConnectionState::ESTABLISHED;
}

void WsConnection::httpClientSend() {
    println("{}:{}: {}", __FILE__, __LINE__, __PRETTY_FUNCTION__);
    string path = "/";
    client_key = create_clientkey();

    auto get = format(
        "GET {} HTTP/1.1\r\n"
        "Host: {}:{}\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: {}\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n",
        path, protocol->local.host, protocol->local.port, client_key);

    ssize_t r = ::send(fd, get.data(), get.size(), 0);
    // println("send http, got {}\n{}", r, get);
    if (r != get.size()) {
        throw runtime_error("failed");
    }
}

void WsConnection::httpServerRcvd() {
    println("{}:{}: {}", __FILE__, __LINE__, __PRETTY_FUNCTION__);
    char buffer[8192];
    ssize_t nbytes = ::recv(fd, buffer, sizeof(buffer), 0);
    if (nbytes < 0) {
        perror("read error");
        return;
    }
    if (nbytes == 0) {
        // ...
    }
    headers.append(buffer, nbytes);
    if (headers.size() > 8192) {
        // upper limit for safety reasons
        // std::cerr << "Too large http header" << std::endl;
    }
    if (headers.find("\r\n\r\n") == std::string::npos) {
        return;
    }
    std::string::size_type keyhdstart;
    // FIXME: needs case insensitive find
    if (headers.find("Upgrade: websocket\r\n") == std::string::npos || headers.find("Connection: Upgrade\r\n") == std::string::npos ||
        (keyhdstart = headers.find("Sec-WebSocket-Key: ")) == std::string::npos) {
        // std::cerr << "http_upgrade: missing required headers" << std::endl;
        // abort
        return;
    }
    keyhdstart += 19;
    std::string::size_type keyhdend = headers.find("\r\n", keyhdstart);
    string client_key2 = headers.substr(keyhdstart, keyhdend - keyhdstart);
    string accept_key = create_acceptkey(client_key2);
    // println("got HTTP request, switching to websocket");

    string reply =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " +
        accept_key +
        "\r\n"
        "\r\n";
    auto r = ::send(fd, reply.data(), reply.size(), 0);
    if (r<0) {
        perror("httpServerRcvd");
    }

    struct wslay_event_callbacks callbacks = {
        wslay_recv_callback,    // called when wslay wants to read data
        wslay_send_callback,    // called when wslay wants to send data
        NULL,                   /* genmask_callback */
        NULL,                   /* on_frame_recv_start_callback */
        NULL,                   /* on_frame_recv_callback */
        NULL,                   /* on_frame_recv_end_callback */
        wslay_msg_rcv_callback  // message received via wslay
    };
    if (wslay_event_context_server_init(&ctx, &callbacks, this) != 0) {
        printf("FAILED TO SETUP SERVER CONTEXT");
    }
    // TODO: call wslay_event_context_free(...) when closing connection

    headers.clear();
    wsstate = WsConnectionState::WS;

    println("{}:{}: {}: SERVER ESTABLISHED WS MODE", __FILE__, __LINE__, __PRETTY_FUNCTION__);
}

void WsConnection::httpClientRcvd() {
    println("{}:{}: {}", __FILE__, __LINE__, __PRETTY_FUNCTION__);
    char buffer[8192];
    ssize_t nbytes = ::recv(fd, buffer, sizeof(buffer), 0);
    if (nbytes < 0) {
        perror("read error");
        return;
    }
    if (nbytes == 0) {
        // ...
    }
    headers.append(buffer, nbytes);
    if (headers.size() > 8192) {
        // upper limit for safety reasons
        // std::cerr << "Too large http header" << std::endl;
    }
    if (headers.find("\r\n\r\n") == std::string::npos) {
        return;
    }

    string &resheader = headers;

    std::string::size_type keyhdstart;
    if ((keyhdstart = resheader.find("Sec-WebSocket-Accept: ")) == std::string::npos) {
        // std::cerr << "http_upgrade: missing required headers" << std::endl;
        return;
    }
    keyhdstart += 22;
    std::string::size_type keyhdend = resheader.find("\r\n", keyhdstart);
    std::string accept_key = resheader.substr(keyhdstart, keyhdend - keyhdstart);
    if (accept_key == create_acceptkey(client_key)) {
        auto body = resheader.substr(resheader.find("\r\n\r\n") + 4);
        println("CLIENT OK: HAVE {} MORE BYTES AFTER HEADER", body.size());
        // return;
    } else {
        println("CLIENT: SERVER SEND INVALID Sec-WebSocket-Accept");
        return;
    }

    struct wslay_event_callbacks callbacks = {
        wslay_recv_callback,    // called when wslay wants to read data
        wslay_send_callback,    // called when wslay wants to send data
        genmask_callback,       /* genmask_callback */
        NULL,                   /* on_frame_recv_start_callback */
        NULL,                   /* on_frame_recv_callback */
        NULL,                   /* on_frame_recv_end_callback */
        wslay_msg_rcv_callback  // message received via wslay
    };
    // printf("HANDLER %p, CTX %p: @0\n", handler, handler->ctx);
    if (wslay_event_context_client_init(&ctx, &callbacks, this) != 0) {
        printf("FAILED TO SETUP CLIENT CONTEXT\n");
    }
    // printf("HANDLER %p, CTX %p: @1\n", handler, handler->ctx);

    headers.clear();
    wsstate = WsConnectionState::WS;
    // handler->sig.resume();

    println("{}:{}: {}: CLIENT ESTABLISHED WS MODE", __FILE__, __LINE__, __PRETTY_FUNCTION__);
}

void WsConnection::canWrite() {
    println("{}:{}: {}", __FILE__, __LINE__, __PRETTY_FUNCTION__);
    switch (wsstate) {
        case WsConnectionState::HTTP_CLIENT:
            httpClientSend();
            stopWriteHandler();
            break;
    }

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
    println("{}WsConnection::canRead()", prefix(this));

    switch (wsstate) {
        case WsConnectionState::HTTP_SERVER:
            httpServerRcvd();
            break;
        case WsConnectionState::HTTP_CLIENT:
            httpClientRcvd();
            break;
        case WsConnectionState::WS:
            println("{}:{}: {}: WS", __FILE__, __LINE__, __PRETTY_FUNCTION__);
            break;
    }

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
    println("{}WsConnection::up(): -> {}", prefix(this), str());
    if (fd >= 0) {
        return;
    }
    fd = connect_to(remote.host.c_str(), remote.port);
    if (fd < 0) {
        println("{}WsConnection::up(): -> PENDING", prefix(this));
        state = ConnectionState::PENDING;
        throw runtime_error(format("WsConnection()::up(): {}: {}", remote.str(), strerror(errno)));
    }

    // TODO: move these into the constructor?
    ev_io_init(&read_watcher, libev_read_cb, fd, EV_READ);
    ev_io_init(&write_watcher, libev_write_cb, fd, EV_WRITE);
    startTimer();

    if (errno == EINPROGRESS) {
        println("{}WsConnection::up(): -> INPROGRESS", prefix(this));
        state = ConnectionState::INPROGRESS;
        startWriteHandler();
    } else {
        println("{}WsConnection::up(): -> ESTABLISHED", prefix(this));
        state = ConnectionState::ESTABLISHED;
    }
    startReadHandler();
}

void WsConnection::libev_read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    auto connection = reinterpret_cast<WsConnection *>(reinterpret_cast<char *>(watcher) - offsetof(WsConnection, read_watcher));
    connection->canRead();
}

void WsConnection::libev_write_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    auto connection = reinterpret_cast<WsConnection *>(reinterpret_cast<char *>(watcher) - offsetof(WsConnection, write_watcher));
    connection->canWrite();
}

void WsConnection::libev_timer_cb(struct ev_loop *loop, struct ev_timer *watcher, int revents) {
    auto connection = reinterpret_cast<WsConnection *>(reinterpret_cast<char *>(watcher) - offsetof(WsConnection, timer_watcher));
    connection->timer();
}

void WsConnection::stopReadHandler() {
    if (!ev_is_active(&read_watcher)) {
        return;
    }
    println("{}stop read handler", prefix(this));
    ev_io_stop(protocol->loop, &read_watcher);
}

void WsConnection::stopWriteHandler() {
    if (!ev_is_active(&write_watcher)) {
        return;
    }
    ev_io_stop(protocol->loop, &write_watcher);
}

void WsConnection::startReadHandler() {
    if (ev_is_active(&read_watcher)) {
        return;
    }
    println("{}start read handler", prefix(this));
    ev_io_start(protocol->loop, &read_watcher);
}

void WsConnection::startWriteHandler() {
    if (ev_is_active(&write_watcher)) {
        return;
    }
    ev_io_start(protocol->loop, &write_watcher);
}

void WsConnection::startTimer() {
    if (timer_active) {
        return;
    }
    println("startTimer");
    timer_active = true;
    ev_timer_init(&timer_watcher, libev_timer_cb, 1, 0.);
    ev_timer_start(protocol->loop, &timer_watcher);
}
void WsConnection::stopTimer() {
    if (!timer_active) {
        return;
    }
    println("stopTimer");
    timer_active = false;
    ev_timer_stop(protocol->loop, &timer_watcher);
}
void WsConnection::timer() {
    println("timer {}", std::to_underlying(state));
    if (state == ConnectionState::INPROGRESS) {
        println("INPROGRESS -> TIMEOUT");
        stopReadHandler();
        ::close(fd);
        fd = -1;
        state = ConnectionState::IDLE;
        while (!interlock.empty()) {
            interlock.resume(interlock.begin()->first, make_exception_ptr(TIMEOUT(0, CORBA::CompletionStatus::NO)));
        }
    }
}

void WsConnection::print() {
    // println(" -> {}", remote.str());
    // println("{}:{} -> {}:{}", protocol->localHost, protocol->localPort, localHost, localPort);
}

// called by wslay to send data to the socket
ssize_t wslay_send_callback(wslay_event_context_ptr ctx, const uint8_t *data, size_t len, int flags, void *user_data) {
    auto handler = reinterpret_cast<WsConnection *>(user_data);

    // println("wslay_send_callback");

    int sflags = 0;
#ifdef MSG_MORE
    if (flags & WSLAY_MSG_MORE) {
        sflags |= MSG_MORE;
    }
#endif  // MSG_MORE

    ssize_t r;
    while ((r = send(handler->fd, (void *)data, len, sflags)) == -1 && errno == EINTR);
    if (r == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            wslay_event_set_error(ctx, WSLAY_ERR_WOULDBLOCK);
        } else {
            wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
        }
    }
    return r;
}

// called by wslay to read data from the socket
ssize_t wslay_recv_callback(wslay_event_context_ptr ctx, uint8_t *data, size_t len, int flags, void *user_data) {

    // println("wslay_recv_callback");

    auto handler = reinterpret_cast<WsConnection *>(user_data);
    ssize_t nbytes = recv(handler->fd, data, len, 0);
    // println("wslay_recv_callback -> {}", nbytes);
    if (nbytes < 0) {
        if (errno != EAGAIN) {
            // println("errno = {}", errno);
            perror("recv error");
        }
    }
    if (nbytes == 0) {
        if (errno == EAGAIN) {
            println("peer closed");
        } else {
            perror("peer might be closing");
        }
        // ev_io_stop(handler->protocol->loop, &handler->watcher);
        // if (close(handler->watcher.fd) != 0) {
        //     perror("close");
        // }
        // handler->orb->close(handler->connection);
        // delete handler;
    }

    //
    // ssize_t r;
    // while ((r = recv(handler->watcher.fd, data, len, 0)) == -1 && errno == EINTR)
    //     ;
    return nbytes;
}

void wslay_msg_rcv_callback(wslay_event_context_ptr ctx, const struct wslay_event_on_msg_recv_arg *arg, void *user_data) {
    // println("wslay_msg_rcv_callback: opcode {}, length {}", arg->opcode, arg->msg_length);
    auto handler = reinterpret_cast<WsConnection *>(user_data);
    switch (arg->opcode) {
        case WSLAY_BINARY_FRAME:
            handler->protocol->orb->socketRcvd(handler, arg->msg, arg->msg_length);
            break;
        case WSLAY_CONNECTION_CLOSE:
            println("-------------------------------------");
            println("wslay_msg_rcv_callback: close, status code: {}", arg->status_code);
            // println("[1]");
            // // wslay_event_context_free(handler->ctx);
            // println("[2]");
            // ev_io_stop(handler->loop, &handler->watcher);
            // println("[3]");
            // if (close(handler->watcher.fd) != 0) {
            //     perror("close");
            // }
            // println("[4]");
            // // handler->orb->close(handler->connection);
            // println("[5]");
            // // delete handler;
            // println("[6]");
            break;
    }
    // arg->msg = nullptr; // THIS NEEDS A CHANGE IN WSLAY
}

int genmask_callback(wslay_event_context_ptr ctx, uint8_t *buf, size_t len, void *user_data) {
    // client_handler_t *ws = (WsConnection *)user_data;
    // ifstream dev_urand_("/dev/urandom");
    // dev_urand_.read((char *)buf, len);
    //   ws->get_random(buf, len);
    return 0;
}

}  // namespace detail
}  // namespace CORBA
