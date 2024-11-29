#include "stream2packet.hh"

#include <print>

#include "../giop.hh"

using namespace std;

namespace CORBA {

namespace detail {

#define DBG(CMD)

char *GIOPStream2Packets::buffer() {
    DBG(println("GIOPStream2Packets::buffer()");)
    DBG(println("    enter                     : offset={}, size={}, reserved={}, messageSize={}", offset, size, reserved, messageSize);)
    if (reserved - size < receiveBufferSize) {
        reserved += receiveBufferSize;
        data = (char *)realloc(data, reserved);
        DBG(println("    reserved additional memory: offset={}, size={}, reserved={}, messageSize={}", offset, size, reserved, messageSize);)
    }
    return data + size;
}

std::span<char> GIOPStream2Packets::message() {
    DBG(println("GIOPStream2Packets::message()");)
    DBG(println("    get message               : offset={}, size={}, reserved={}, messageSize={}", offset, size, reserved, messageSize);)
    if (messageSize == 0 && size - offset >= 16) {
        CORBA::CDRDecoder cdr(data + offset, size - offset);
        CORBA::GIOPDecoder giop(cdr);
        giop.scanGIOPHeader();
        messageSize = giop.m_length + 12;
        DBG(println("    got message size          : offset={}, size={}, reserved={}, messageSize={}", offset, size, reserved, messageSize);)
    }
    if (messageSize == 0) {
        DBG(println("    no more messages          : offset={}, size={}, reserved={}, messageSize={}", offset, size, reserved, messageSize);)
        return {};
    }
    if (messageSize != 0 && size - offset < messageSize) {
        // incomplete message at buffer end, move to front of buffer
        DBG(println("    no more messages, move    : offset={}, size={}, reserved={}, messageSize={}", offset, size, reserved, messageSize);)
        if (offset > 0) {
            DBG(println("      move(0, {}, {})", offset, size - offset);)
            memmove(data, data + offset, size - offset);
        }
        size -= offset;
        offset = 0;
        DBG(println("    moved                     : offset={}, size={}, reserved={}, messageSize={}", offset, size, reserved, messageSize);)
        return {};
    }
    span result(data + offset, data + offset + messageSize);
    offset += messageSize;
    messageSize = 0;
    if (offset == size) {
        offset = size = 0;
        DBG(println("    offset reached end, reset : offset={}, size={}, reserved={}, messageSize={}", offset, size, reserved, messageSize);)
    }
    DBG(println("    return message            : offset={}, size={}, reserved={}, messageSize={}", offset, size, reserved, messageSize);)
    return result;
}

}  // namespace detail
}  // namespace CORBA
