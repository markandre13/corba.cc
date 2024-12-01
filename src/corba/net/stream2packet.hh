#pragma once

#include <cstdlib>
#include <span>

namespace CORBA {

namespace detail {

// TODO: constexpr???
#define DBG(CMD)

/**
 * Am internal helper class to convert a byte stream into CORBA IIOP packets.
 */
class IIOPStream2Packet {
    public:
        size_t receiveBufferSize = 0x20000;
        char *data = nullptr;
        size_t size = 0;
        size_t reserved = 0;

        size_t offset = 0;
        size_t messageSize = 0;

        IIOPStream2Packet(size_t receiveBufferSize = 0x20000) : receiveBufferSize(receiveBufferSize) {}
        ~IIOPStream2Packet() { free(data); }

        /**
         * get buffer for next read operation
         */
        char *buffer();

        /**
         * get size of buffer returned by buffer()
         */
        inline size_t length() { return reserved - size; }

        /**
         * inform about how many bytes have been read into buffer
         */
        inline void received(size_t nbytes) {
            DBG(println("IIOPStream2Packet::received({})", nbytes);)
            size += nbytes;
            DBG(println("    received more data        : offset={}, size={}, reserved={}, messageSize={}", offset, size, reserved, messageSize);)
        }
        /**
         * return one CORBA IIOP packet or an empty span
         */
        std::span<char> message();
};

#undef DGB

}  // namespace detail
}  // namespace CORBA
