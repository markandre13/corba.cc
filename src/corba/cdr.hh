#pragma once

#include <bit>
#include <cctype>
#include <format>
#include <string>
#include <vector>

#include "blob.hh"

namespace CORBA {

// Common Data Representation

class CDREncoder {
    public:
        std::vector<char> _data;
        size_t offset = 0;

    protected:
        std::vector<size_t> sizeStack;

    public:
        inline void reserve(size_t nbytes) {
            if (_data.size() < nbytes) {
                _data.resize(nbytes);
            }
        }
        inline void reserve() { reserve(offset); }

        void writeBoolean(bool);
        void writeOctet(uint8_t);
        void writeUshort(uint16_t);
        void writeUlong(uint32_t);
        void writeUlonglong(uint64_t);
        void writeShort(int16_t);
        void writeLong(int32_t);
        void writeLonglong(int64_t);
        void writeFloat(float);
        void writeDouble(double);
        void writeBlob(const char *buffer, size_t nbytes);
        void writeString(const char *buffer);
        void writeString(const char *buffer, size_t nbytes);
        inline void writeString(const std::string &value) { writeString(value.data(), value.size()); }
        inline void writeString(const std::string_view &value) { writeString(value.data(), value.size()); }
        void writeEndian();

        void reserveSize();
        void fillInSize();

        const char *data() { return _data.data(); }
        size_t length() { return _data.size(); }

        void align2() {
            if (offset & 0x01) {
                offset |= 0x01;
                ++offset;
            }
        }
        void align4() {
            if (offset & 0x03) {
                offset |= 0x03;
                ++offset;
            }
        }
        void align8() {
            if (offset & 0x07) {
                offset |= 0x07;
                ++offset;
            }
        }
};

class CDRDecoder {
    public:
        const char *_data;
        size_t m_offset;
        size_t length;
        std::endian _endian;

        CDRDecoder() : _data(nullptr), m_offset(0), length(0) {}
        CDRDecoder(const char *data, size_t length, std::endian endian = (std::endian)0) : _data(data), m_offset(0), length(length), _endian(endian) {}
        CDRDecoder(CDREncoder &encoder) : _data(encoder.data()), m_offset(0), length(encoder.length()), _endian(std::endian::native) {}
        std::string_view toString() const { return std::string_view(_data, length); }
        std::string str() const { return std::string(_data, length); }

        void readEndian() { setLittleEndian(readOctet() & 1); }
        bool readBoolean();
        uint8_t readOctet();
        char8_t readChar();
        uint16_t readUshort();
        uint32_t readUlong();
        uint64_t readUlonglong();
        int16_t readShort();
        int32_t readLong();
        int64_t readLonglong();
        float readFloat();
        double readDouble();
        CORBA::blob readBlob();
        std::string readString();
        std::string readString(size_t length);
        CORBA::blob_view readBlobView();
        std::string_view readStringView();
        std::string_view readStringView(size_t length);

        // sequence
        // value
        // object
        // reference

        bool operator==(const CDRDecoder &rhs) const;
        const char *data() const { return _data; }
        void skip(size_t size) { m_offset += size; }
        void setLittleEndian(bool little) { _endian = little ? std::endian::little : std::endian::big; }
        void setOffset(size_t offset) { this->m_offset = offset; }
        size_t getOffset() const { return m_offset; }
        void align2() {
            if (m_offset & 0x01) {
                m_offset |= 0x01;
                ++m_offset;
            }
        }
        void align4() {
            if (m_offset & 0x03) {
                m_offset |= 0x03;
                ++m_offset;
            }
        }
        void align8() {
            if (m_offset & 0x07) {
                m_offset |= 0x07;
                ++m_offset;
            }
        }

    protected:
        const char *ptr2() {
            align2();
            auto ptr = _data + m_offset;
            m_offset += 2;
            if (m_offset > length) {
                throw std::out_of_range("out of range");
            }
            return ptr;
        }
        const char *ptr4() {
            align4();
            auto ptr = _data + m_offset;
            m_offset += 4;
            if (m_offset > length) {
                throw std::out_of_range(std::format("out of range in CDRDecoder::ptr4(): offset {} > length {}", m_offset, length));
            }
            return ptr;
        }
        const char *ptr8() {
            align8();
            auto ptr = _data + m_offset;
            m_offset += 8;
            if (m_offset > length) {
                throw std::out_of_range("out of range");
            }
            return ptr;
        }
};

}  // namespace CORBA
