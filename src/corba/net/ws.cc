#include "ws.hh"

#include "../orb.hh"
#include "ws/createAcceptKey.hh"
#include "ws/socket.hh"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>  // for puts
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include "../../../upstream/wslay/lib/wslay_event.h"

#include <fstream>
#include <iostream>
#include <print>

using namespace std;

namespace CORBA {

namespace net {

static void libev_accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
static void libev_read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);

static ssize_t wslay_send_callback(wslay_event_context_ptr ctx, const uint8_t *data, size_t len, int flags, void *user_data);
static ssize_t wslay_recv_callback(wslay_event_context_ptr ctx, uint8_t *data, size_t len, int flags, void *user_data);
static void wslay_msg_rcv_callback(wslay_event_context_ptr ctx, const struct wslay_event_on_msg_recv_arg *arg, void *user_data);

// libev user data for the listen handler
struct listen_handler_t {
        ev_io watcher;
        struct ev_loop *loop;
        WsProtocol *protocol;
};

// libev user data for the client handler
enum state_t { STATE_HTTP_SERVER, STATE_HTTP_CLIENT, STATE_WS };
struct client_handler_t {
        ev_io watcher;
        struct ev_loop *loop;
        signal sig;

        // for initial HTTP negotiation
        state_t state = STATE_HTTP_SERVER;
        std::string headers;
        std::string client_key;

        // wslay context for established connections
        wslay_event_context_ptr ctx;

        // for forwarding data from wslay to corba
        CORBA::ORB *orb;
        WsConnection *connection;
};

/**
 * Add a listen socket for the specified hostname and port to the libev loop
 */
void WsProtocol::listen(CORBA::ORB *orb, struct ev_loop *loop, const std::string &hostname, uint16_t port) {
    m_localAddress = hostname;
    m_localPort = port;
    m_orb = orb;
    m_loop = loop;

    int fd = create_listen_socket(hostname.c_str(), port);
    if (fd < 0) {
        throw runtime_error(format("WsProtocol::listen(): {}:{}: {}", hostname, port, strerror(errno)));
    }
    auto accept_watcher = new listen_handler_t;
    accept_watcher->loop = loop;
    accept_watcher->protocol = this;
    ev_io_init(&accept_watcher->watcher, libev_accept_cb, fd, EV_READ);
    ev_io_start(loop, &accept_watcher->watcher);
}

/**
 * Attach the protocol to the libev loop but do not open a server socket.
 */
void WsProtocol::attach(CORBA::ORB *orb, struct ev_loop *loop) {
    m_orb = orb;
    m_loop = loop;
}

// called by libev when a client want's to connect
void libev_accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    auto handler = reinterpret_cast<listen_handler_t *>(watcher);
    puts("got client");
    if (EV_ERROR & revents) {
        perror("got invalid event");
        return;
    }

    int fd = accept(watcher->fd, 0, 0);

    if (set_non_block(fd) == -1 || set_no_delay(fd) == -1) {
        puts("failed to setup");
        close(fd);
        return;
    }

    auto client_handler = new client_handler_t();
    client_handler->loop = loop;
    client_handler->orb = handler->protocol->m_orb;
    // a serve will only send requests when BiDir was negotiated, and then starts with
    // requestId 1 and increments by 2
    // (while client starts with requestId 0 and also increments by 2)
    auto InitialResponderRequestIdBiDirectionalIIOP = 1;
    client_handler->connection =
        new WsConnection(handler->protocol->m_localAddress, handler->protocol->m_localPort, "frontend", 2, InitialResponderRequestIdBiDirectionalIIOP);
    handler->protocol->m_orb->connections.push_back(client_handler->connection);
    client_handler->connection->handler = client_handler;
    ev_io_init(&client_handler->watcher, libev_read_cb, fd, EV_READ);
    ev_io_start(loop, &client_handler->watcher);
}

async<detail::Connection *> WsProtocol::connect(const CORBA::ORB *orb, const std::string &hostname, uint16_t port) {
    println("WsProtocol::connect(orb, \"{}\", {})", hostname, port);

    int fd = connect_to(hostname.c_str(), port);
    int val = 1;
    if (set_non_block(fd) == -1 || set_no_delay(fd) == -1) {
        puts("failed to setup");
        ::close(fd);
        co_return nullptr;
    }

    auto client_handler = new client_handler_t();
    client_handler->state = STATE_HTTP_CLIENT;
    client_handler->loop = m_loop;
    client_handler->orb = m_orb;
    // a server will only send requests when BiDir was negotiated, and then starts with
    // requestId 1 and increments by 2
    // (while client starts with requestId 0 and also increments by 2)
    // auto InitialResponderRequestIdBiDirectionalIIOP = 1;

    if (m_localAddress.empty()) {
        // this orb/protocol has no listen port, assume bi-directional iiop and makeup a hostname
        // using a random number also helps avoiding collisions with other orbs on the peer
        uuid_t uuid;
        uuid_string_t uuid_str;
        uuid_generate_random(uuid);
        uuid_unparse_lower(uuid, uuid_str);
        m_localAddress = uuid_str;

        struct sockaddr_in my_addr;
        bzero(&my_addr, sizeof(my_addr));
        socklen_t len = sizeof(my_addr);
        getsockname(fd, (struct sockaddr *)&my_addr, &len);
        m_localPort = ntohs(my_addr.sin_port);
    }
    println("CONNECT LOCAL SOCKET IS {}:{}", m_localAddress, m_localPort);

    string path = "/";
    client_handler->client_key = create_clientkey();

    auto get = format(
        "GET {} HTTP/1.1\r\n"
        "Host: {}:{}\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: {}\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n",
        path, hostname, port, client_handler->client_key);

    ssize_t r = send(fd, get.data(), get.size(), 0);
    // println("send http, got {}\n{}", r, get);
    if (r != get.size()) {
        throw runtime_error("failed");
    }

    client_handler->connection = new WsConnection(m_localAddress, m_localPort, hostname, port);
    client_handler->connection->handler = client_handler;
    ev_io_init(&client_handler->watcher, libev_read_cb, fd, EV_READ);
    ev_io_start(m_loop, &client_handler->watcher);

    println("suspend WsProtocol::connect()");
    co_await client_handler->sig.suspend();
    println("resume WsProtocol::connect()");

    co_return client_handler->connection;
}

int genmask_callback(wslay_event_context_ptr ctx, uint8_t *buf, size_t len, void *user_data) {
    client_handler_t *ws = (client_handler_t *)user_data;
    ifstream dev_urand_("/dev/urandom");
    dev_urand_.read((char *)buf, len);
    //   ws->get_random(buf, len);
    return 0;
}

CORBA::async<void> WsProtocol::close() { co_return; }
void WsConnection::close(){};

// called by libev when data can be read
void libev_read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    if (EV_ERROR & revents) {
        perror("libev_read_cb(): got invalid event");
        return;
    }
    auto handler = reinterpret_cast<client_handler_t *>(watcher);
    switch (handler->state) {
        case STATE_WS:
            wslay_event_recv(handler->ctx);
            break;

        case STATE_HTTP_CLIENT:
        case STATE_HTTP_SERVER: {
            char buffer[8192];
            ssize_t nbytes = recv(watcher->fd, buffer, 8192, 0);
            if (nbytes < 0) {
                perror("read error");
                return;
            }
            if (nbytes == 0) {
                if (errno == EINVAL) {
                    println("peer closed");
                } else {
                    perror("peer might be closing");
                }
                ev_io_stop(loop, watcher);
                if (close(watcher->fd) != 0) {
                    perror("close");
                }
                delete handler;
                return;
            }

            println("rcvd {} bytes", nbytes);
            handler->headers.append(buffer, nbytes);
            if (handler->headers.size() > 8192) {
                std::cerr << "Too large http header" << std::endl;
            }
            switch (handler->state) {
                case STATE_HTTP_CLIENT: {
                    if (handler->headers.find("\r\n\r\n") != std::string::npos) {
                        println("HTTP:CLIENT received http\n{}\n\n", handler->headers);

                        string& resheader = handler->headers;

                        std::string::size_type keyhdstart;
                        if ((keyhdstart = resheader.find("Sec-WebSocket-Accept: ")) == std::string::npos) {
                            std::cerr << "http_upgrade: missing required headers" << std::endl;
                            return;
                        }
                        keyhdstart += 22;
                        std::string::size_type keyhdend = resheader.find("\r\n", keyhdstart);
                        std::string accept_key = resheader.substr(keyhdstart, keyhdend - keyhdstart);
                        if (accept_key == create_acceptkey(handler->client_key)) {
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
                        printf("HANDLER %p, CTX %p: @0\n", handler, handler->ctx);
                        if (wslay_event_context_client_init(&handler->ctx, &callbacks, handler) != 0) {
                            printf("FAILED TO SETUP CLIENT CONTEXT\n");
                        }
                        printf("HANDLER %p, CTX %p: @1\n", handler, handler->ctx);

                        handler->headers.clear();
                        handler->state = STATE_WS;
                        handler->sig.resume();
                    }
                } break;
                case STATE_HTTP_SERVER:
                    if (handler->headers.find("\r\n\r\n") != std::string::npos) {
                        std::string::size_type keyhdstart;
                        if (handler->headers.find("Upgrade: websocket\r\n") == std::string::npos ||
                            handler->headers.find("Connection: Upgrade\r\n") == std::string::npos ||
                            (keyhdstart = handler->headers.find("Sec-WebSocket-Key: ")) == std::string::npos) {
                            std::cerr << "http_upgrade: missing required headers" << std::endl;
                            // abort
                            return;
                        }
                        keyhdstart += 19;
                        std::string::size_type keyhdend = handler->headers.find("\r\n", keyhdstart);
                        string client_key = handler->headers.substr(keyhdstart, keyhdend - keyhdstart);
                        string accept_key = create_acceptkey(client_key);
                        println("got HTTP request, switching to websocket");

                        handler->headers.clear();
                        handler->state = STATE_WS;

                        string reply =
                            "HTTP/1.1 101 Switching Protocols\r\n"
                            "Upgrade: websocket\r\n"
                            "Connection: Upgrade\r\n"
                            "Sec-WebSocket-Accept: " +
                            accept_key +
                            "\r\n"
                            "\r\n";
                        send(watcher->fd, reply.data(), reply.size(), 0);

                        struct wslay_event_callbacks callbacks = {
                            wslay_recv_callback,    // called when wslay wants to read data
                            wslay_send_callback,    // called when wslay wants to send data
                            NULL,                   /* genmask_callback */
                            NULL,                   /* on_frame_recv_start_callback */
                            NULL,                   /* on_frame_recv_callback */
                            NULL,                   /* on_frame_recv_end_callback */
                            wslay_msg_rcv_callback  // message received via wslay
                        };
                        if (wslay_event_context_server_init(&handler->ctx, &callbacks, handler) != 0) {
                            printf("FAILED TO SETUP SERVER CONTEXT");
                        }
                        // TODO: call wslay_event_context_free(...) when closing connection
                    }
                    break;
            }
        } break;
    }
}

// this is called by the ORB to send data
void WsConnection::send(void *buffer, size_t nbyte) {
    println("WsConnection::send(..., {})", nbyte);
    struct wslay_event_msg msgarg = {WSLAY_BINARY_FRAME, (const uint8_t *)buffer, nbyte};
    int r0 = wslay_event_queue_msg(this->handler->ctx, &msgarg);
    println("wslay_event_queue_msg() -> {}", r0);
    switch (r0) {
        case 0: {
            // send queued messages
            // the proper approach would be to temporarily register a write callback
            // now that we have something to write like this
            //
            //   ev_io_init(&client_handler->watcher, libev_write_cb, fd, EV_WRITE);
            //
            // and then call wslay_event_send() from there and remove the write callback
            // again. this way we won't get blocked on writes.
            printf("HANDLER %p, CTX %p: @3\n", handler, handler->ctx);
            int r1 = wslay_event_send(this->handler->ctx);
            println("wslay_event_send() -> {}", r1);
        } break;
        case WSLAY_ERR_NO_MORE_MSG:
            cout << "WSLAY_ERR_NO_MORE_MSG: Could not queue given message." << endl
                 << "The one of possible reason is that close control frame has been queued/sent" << endl
                 << "and no further queueing message is not allowed." << endl;
        case WSLAY_ERR_INVALID_ARGUMENT:
            cout << "WSLAY_ERR_INVALID_ARGUMENT: The given message is invalid." << endl;
            break;
        case WSLAY_ERR_NOMEM:
            cout << "WSLAY_ERR_NOMEM Out of memory." << endl;
            break;
        default:
            cout << "failed to queue wslay message" << endl;
            //     std::cout << "SEND FACE " << r << std::endl;
            break;
    }
}

// called by wslay to send data to the socket
ssize_t wslay_send_callback(wslay_event_context_ptr ctx, const uint8_t *data, size_t len, int flags, void *user_data) {
    auto handler = reinterpret_cast<client_handler_t *>(user_data);
    println("wslay_send_callback");
    auto r = send(handler->watcher.fd, (void *)data, len, 0);
    println("send() -> {}", r);
    return r;
}

// called by wslay to read data from the socket
ssize_t wslay_recv_callback(wslay_event_context_ptr ctx, uint8_t *data, size_t len, int flags, void *user_data) {
    auto handler = reinterpret_cast<client_handler_t *>(user_data);
    ssize_t nbytes = recv(handler->watcher.fd, data, len, 0);
    println("wslay_recv_callback -> {}", nbytes);
    if (nbytes < 0) {
        if (errno != EAGAIN) {
            println("errno = {}", errno);
            perror("recv error");
        }
    }
    if (nbytes == 0) {
        if (errno == EAGAIN) {
            println("peer closed");
        } else {
            perror("peer might be closing");
        }
        ev_io_stop(handler->loop, &handler->watcher);
        if (close(handler->watcher.fd) != 0) {
            perror("close");
        }
        delete handler;
    }

    //
    // ssize_t r;
    // while ((r = recv(handler->watcher.fd, data, len, 0)) == -1 && errno == EINTR)
    //     ;
    return nbytes;
}

void wslay_msg_rcv_callback(wslay_event_context_ptr ctx, const struct wslay_event_on_msg_recv_arg *arg, void *user_data) {
    println("wslay_msg_rcv_callback");
    auto handler = reinterpret_cast<client_handler_t *>(user_data);
    handler->orb->socketRcvd(handler->connection, arg->msg, arg->msg_length);
    // arg->msg = nullptr; // THIS NEEDS A CHANGE IN WSLAY
}

}  // namespace net

}  // namespace CORBA
