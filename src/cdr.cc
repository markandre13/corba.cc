#include "cdr.hh"

#include <bit>
#include <format>
#include <iostream>

using namespace std;

namespace CORBA {

void CDREncoder::writeBoolean(bool value) {
    reserve(offset + 1);
    auto ptr = reinterpret_cast<uint8_t *>(_data.data() + offset);
    offset += 1;
    *ptr = value ? 1 : 0;
}

void CDREncoder::writeOctet(uint8_t value) {
    reserve(offset + 1);
    auto ptr = reinterpret_cast<uint8_t *>(_data.data() + offset);
    offset += 1;
    *ptr = value;
}

void CDREncoder::writeUshort(uint16_t value) {
    align2();
    reserve(offset + 2);
    auto ptr = reinterpret_cast<uint16_t *>(_data.data() + offset);
    offset += 2;
    *ptr = value;
}

void CDREncoder::writeUlong(uint32_t value) {
    align4();
    reserve(offset + 4);
    auto ptr = reinterpret_cast<uint32_t *>(_data.data() + offset);
    offset += 4;
    *ptr = value;
}

void CDREncoder::writeUlonglong(uint64_t value) {
    align8();
    reserve(offset + 8);
    auto ptr = reinterpret_cast<uint64_t *>(_data.data() + offset);
    offset += 8;
    *ptr = value;
}

void CDREncoder::writeShort(int16_t value) {
    align2();
    reserve(offset + 2);
    auto ptr = reinterpret_cast<int16_t *>(_data.data() + offset);
    offset += 2;
    *ptr = value;
}

void CDREncoder::writeLong(int32_t value) {
    align4();
    reserve(offset + 4);
    auto ptr = reinterpret_cast<int32_t *>(_data.data() + offset);
    offset += 4;
    *ptr = value;
}

void CDREncoder::writeLonglong(int64_t value) {
    align8();
    reserve(offset + 8);
    auto ptr = reinterpret_cast<int64_t *>(_data.data() + offset);
    offset += 8;
    *ptr = value;
}

void CDREncoder::writeFloat(float value) {
    align4();
    reserve(offset + 4);
    auto ptr = reinterpret_cast<float *>(_data.data() + offset);
    offset += 4;
    *ptr = value;
}

void CDREncoder::writeDouble(double value) {
    align8();
    reserve(offset + 8);
    auto ptr = reinterpret_cast<double *>(_data.data() + offset);
    offset += 8;
    *ptr = value;
}

void CDREncoder::writeBlob(const char *value, size_t nbytes) {
    writeUlong(nbytes);
    reserve(offset + nbytes);
    memcpy(_data.data() + offset, value, nbytes);
    offset += nbytes;
}

void CDREncoder::writeString(const char *value) { writeString(value, strlen(value)); }
void CDREncoder::writeString(const char *value, size_t nbytes) {
    writeUlong(nbytes + 1);
    reserve(offset + nbytes + 1);
    memcpy(_data.data() + offset, value, nbytes);
    offset += nbytes;
    _data[offset] = 0;
    ++offset;
}

void CDREncoder::writeEndian() { writeOctet(endian::native == endian::big ? 0 : 1); }

void CDREncoder::reserveSize() {
    align4();
    offset += 4;
    sizeStack.push_back(offset);
}
void CDREncoder::fillInSize() {
    if (sizeStack.empty()) {
        throw runtime_error("internal error: fillinSize() misses reserveSize()");
    }
    auto currrentOffset = offset;
    auto savedOffset = sizeStack.back();
    sizeStack.pop_back();
    offset = savedOffset - 4;
    auto size = currrentOffset - savedOffset;
    writeUlong(size);
    offset = currrentOffset;
}

bool CDRDecoder::operator==(const CDRDecoder &rhs) const {
    if (this == &rhs) {
        return true;
    }
    if (length != rhs.length) {
        return false;
    }
    if (_data == rhs._data) {
        return true;
    }
    return memcmp(_data, rhs._data, length) == 0;
}

bool CDRDecoder::readBoolean() {
    auto value = _data[m_offset] != 0;
    m_offset += 1;
    return value;
}

uint8_t CDRDecoder::readOctet() {
    auto value = _data[m_offset];
    m_offset += 1;
    return value;
}

char8_t CDRDecoder::readChar() {
    auto value = (char8_t)_data[m_offset];
    m_offset += 1;
    return value;
}

uint16_t CDRDecoder::readUshort() {
    auto value = *reinterpret_cast<const uint16_t *>(ptr2());
    if (std::endian::native != _endian) {
        value = __builtin_bswap16(value);
    }
    return value;
}

uint32_t CDRDecoder::readUlong() {
    auto ptr = reinterpret_cast<const uint32_t *>(ptr4());
    uint32_t value = *ptr;
    if (std::endian::native != _endian) {
        value = __builtin_bswap32(value);
    }
    return value;
}

uint64_t CDRDecoder::readUlonglong() {
    auto value = *reinterpret_cast<const uint64_t *>(ptr8());
    if (std::endian::native != _endian) {
        value = __builtin_bswap64(value);
    }
    return value;
}

int16_t CDRDecoder::readShort() {
    auto value = *reinterpret_cast<const int16_t *>(ptr2());
    if (std::endian::native != _endian) {
        value = __builtin_bswap16(value);
    }
    return value;
}

int32_t CDRDecoder::readLong() {
    auto ptr = reinterpret_cast<const int32_t *>(ptr4());
    uint32_t value = *ptr;
    if (std::endian::native != _endian) {
        value = __builtin_bswap32(value);
    }
    return value;
}

int64_t CDRDecoder::readLonglong() {
    auto value = *reinterpret_cast<const int64_t *>(ptr8());
    if (std::endian::native != _endian) {
        value = __builtin_bswap64(value);
    }
    return value;
}

float CDRDecoder::readFloat() {
    auto ptr = reinterpret_cast<const float *>(ptr4());
    float value = *ptr;
    if (std::endian::native != _endian) {
        value = __builtin_bswap32(value);
    }
    return value;
}

double CDRDecoder::readDouble() {
    auto value = *reinterpret_cast<const double *>(ptr8());
    if (std::endian::native != _endian) {
        value = __builtin_bswap64(value);
    }
    return value;
}

CORBA::blob CDRDecoder::readBlob() {
    size_t len = readUlong();
    auto buffer = _data + m_offset;
    m_offset += len;
    if (m_offset > length) {
        throw std::out_of_range("out of range");
    }
    return CORBA::blob(buffer, len);
}

std::string CDRDecoder::readString() { return readString(readUlong()); }

std::string CDRDecoder::readString(size_t len) {
    auto buffer = _data + m_offset;
    m_offset += len;
    if (m_offset > length) {
        throw std::out_of_range("out of range");
    }
    return std::string(buffer, len - 1);
}

CORBA::blob_view CDRDecoder::readBlobView() {
    size_t len = readUlong();
    auto buffer = _data + m_offset;
    m_offset += len;
    if (m_offset > length) {
        throw std::out_of_range("out of range");
    }
    return CORBA::blob_view(buffer, len);
}

std::string_view CDRDecoder::readStringView() { return readStringView(readUlong()); }

std::string_view CDRDecoder::readStringView(size_t len) {
    auto buffer = _data + m_offset;
    m_offset += len;
    if (m_offset > length) {
        throw std::out_of_range("out of range");
    }
    return std::string_view(buffer, len - 1);
}

}  // namespace CORBA
