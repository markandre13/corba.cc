#pragma once

#include <stdint.h>

#include <functional>
#include <string>

#include "cdr.hh"

namespace CORBA {

class ORB;
class Object;
class IOR;

enum class MessageType {
    REQUEST = 0,
    REPLY = 1,
    CANCEL_REQUEST = 2,
    LOCATE_REQUEST = 3,
    LOCATE_REPLY = 4,
    CLOSE_CONNECTION = 5,
    MESSAGE_ERROR = 6,
    FRAGMENT = 7
};

enum class ReplyStatus {
    NO_EXCEPTION = 0,
    USER_EXCEPTION = 1,
    SYSTEM_EXCEPTION = 2,
    LOCATION_FORWARD = 3,
    // since GIOP 1.2
    LOCATION_FORWARD_PERM = 4,
    NEEDS_ADDRESSING_MODE = 5
};

enum class ServiceId {
    TransactionService = 0,
    CodeSets = 1,
    ChainBypassCheck = 2,
    ChainBypassInfo = 3,
    LogicalThreadId = 4,
    BI_DIR_IIOP = 5,
    SendingContextRunTime = 6,
    INVOCATION_POLICIES = 7,
    FORWARDED_IDENTITY = 8,
    UnknownExceptionInfo = 9,
    RTCorbaPriority = 10,
    RTCorbaPriorityRange = 11,
    FT_GROUP_VERSION = 12,
    FT_REQUEST = 13,
    ExceptionDetailMessage = 14,
    SecurityAttributeService = 15,  // CSIv2
    ActivityService = 16,
    RMICustomMaxStreamFormat = 17,
    ACCESS_SESSION_ID = 18,
    SERVICE_SESSION_ID = 19,
    FIREWALL_PATH = 20,
    FIREWALL_PATH_RESP = 21,

    // JacORB uses this as the last context to fill to an 8 byte boundary
    SERVICE_PADDING_CONTEXT = 0x4a414301  // "JAC\01"
};

// CORBA 3.3 Part 2: 7.6.4 Standard IOR Profiles
enum class ProfileId { TAG_INTERNET_IOP = 0, TAG_MULTIPLE_COMPONENTS = 1, TAG_SCCP_IOP = 2, TAG_UIPMC = 3, TAG_MOBILE_TERMINAL_IOP = 4 };

// CORBA 3.3 Part 2: 7.6.6 Standard IOP Components
enum class ComponentId {
    ORB_TYPE = 0,
    CODE_SETS = 1,
    POLICIES = 2,
    ALTERNATE_IIOP_ADDRESS = 3,
    COMPLETE_OBJECT_KEY = 5,
    ENDPOINT_ID_POSITION = 6,
    LOCATION_POLICY = 12,
    ASSOCIATION_OPTIONS = 13,
    SEC_NAME = 14,
    SPKM_1_SEC_MECH = 15,
    SPKM_2_SEC_MECH = 16,
    KerberosV5_SEC_MECH = 17,
    CSI_ECMA_Secret_SEC_MECH = 18,
    CSI_ECMA_Hybrid_SEC_MECH = 19,
    SSL_SEC_TRANS = 20,
    CSI_ECMA_Public_SEC_MECH = 21,
    GENERIC_SEC_MECH = 22,
    FIREWALL_TRANS = 23,
    SCCP_CONTACT_INFO = 24,
    JAVA_CODEBASE = 25,
    TRANSACTION_POLICY = 26,
    MESSAGE_ROUTERS = 30,
    OTS_POLICY = 31,
    INV_POLICY = 32,
    CSI_SEC_MECH_LIST = 33,
    NULL_TAG = 34,
    SECIOP_SEC_TRANS = 35,
    TLS_SEC_TRANS = 36,
    ACTIVITY_POLICY = 37,
    RMI_CUSTOM_MAX_STREAM_FORMAT = 38,
    GROUP = 39,
    GROUP_IIOP = 40,
    PASSTHRU_TRANS = 41,
    FIREWALL_PATH = 42,
    IIOP_SEC_TRANS = 43,
    DCE_STRING_BINDING = 100,
    DCE_BINDING_NAME = 101,
    DCE_NO_PIPES = 102,
    DCE_SEC_MECH = 103,
    INET_SEC_TRANS = 123
};

enum class AddressingDisposition { KEY_ADDR = 0, PROFILE_ADDR = 1, REFERENCE_ADDR = 2 };

struct GIOPHeader {
        char id[4];
        uint8_t majorVersion;
        uint8_t minorVersion;
        uint8_t endian;
        uint8_t type;
        uint32_t length;
};

struct RequestHeader {
        uint32_t requestId;
        bool responseExpected;
        blob_view objectKey;
        std::string_view operation;
};

struct ReplyHeader {
        uint32_t requestId;
        ReplyStatus replyStatus;
};

struct LocateRequest {
        uint32_t requestId;
        blob_view objectKey;
        LocateRequest(uint32_t requestId, const blob_view &objectKey) : requestId(requestId), objectKey(objectKey) {}
};

namespace detail {
class Connection;
}

class GIOPBase {
    public:
        GIOPBase(detail::Connection *connection = nullptr) : connection(connection) {}
        unsigned majorVersion = 1;
        unsigned minorVersion = 2;

        const unsigned ENDIAN_LITTLE = 0;
        const unsigned ENDIAN_BIG = 1;

        detail::Connection *connection = nullptr;
};

class GIOPEncoder : public GIOPBase {
    public:
        GIOPEncoder(detail::Connection *connection = nullptr) : GIOPBase(connection) { this->connection = connection; }

        CDREncoder buffer;
        inline void writeBoolean(bool value) { buffer.writeBoolean(value); }
        inline void writeOctet(u_int8_t value) { buffer.writeOctet(value); }
        inline void writeUshort(uint16_t value) { buffer.writeUshort(value); }
        inline void writeUlong(uint32_t value) { buffer.writeUlong(value); }
        inline void writeUlonglong(uint64_t value) { buffer.writeUlonglong(value); }
        inline void writeShort(int16_t value) { buffer.writeShort(value); }
        inline void writeLong(int32_t value) { buffer.writeLong(value); }
        inline void writeLonglong(int64_t value) { buffer.writeLonglong(value); }
        inline void writeFloat(float value) { buffer.writeFloat(value); }
        inline void writeDouble(double value) { buffer.writeDouble(value); }

        inline void writeString(const char *value) { buffer.writeString(value); }
        inline void writeString(const char *value, size_t size) { buffer.writeString(value, size); }
        inline void writeString(const std::string &value) { buffer.writeString(value); }
        inline void writeString(const std::string_view &value) { buffer.writeString(value); }

        inline void writeBlob(const char *value, size_t size) { buffer.writeBlob(value, size); }
        inline void writeBlob(const std::string &value) { buffer.writeBlob(value.data(), value.size()); }
        inline void writeBlob(const std::string_view &value) { buffer.writeBlob(value.data(), value.size()); }
        inline void writeBlob(const CORBA::blob &value) { buffer.writeBlob((const char*)value.data(), value.size()); }
        inline void writeBlob(const CORBA::blob_view &value) { buffer.writeBlob((const char*)value.data(), value.size()); }

        inline void writeEndian() { buffer.writeEndian(); }

        inline void reserveSize() { buffer.reserveSize(); }
        inline void fillInSize() { buffer.fillInSize(); }

        void writeObject(const Object *object);
        void writeReference(const Object *object);
        void writeEncapsulation(ComponentId type, std::function<void()> closure);
        void writeEncapsulation(ProfileId type, std::function<void()> closure);
        void writeEncapsulation(ServiceId type, std::function<void()> closure);
        void skipGIOPHeader();
        void skipReplyHeader();
        void setGIOPHeader(MessageType type);
        void setReplyHeader(uint32_t requestId, CORBA::ReplyStatus replyStatus);
        void encodeRequest(const CORBA::blob &objectKey, const std::string &operation, uint32_t requestId, bool responseExpected);
        void writeServiceContext();
};

class GIOPDecoder : public GIOPBase {
    public:
        CDRDecoder &buffer;
        MessageType m_type;
        size_t m_length;

        uint32_t requestId;
        ReplyStatus replyStatus;

        GIOPDecoder(CDRDecoder &buffer) : buffer(buffer) {}
        MessageType scanGIOPHeader();
        const RequestHeader *scanRequestHeader();
        const LocateRequest *scanLocateRequest();
        std::unique_ptr<ReplyHeader> scanReplyHeader();

        void readServiceContext();

        // CORBA 3.4 Part 2, 9.3.3 Encapsulation
        // Used for ServiceContext, Profile and Component
        void readEncapsulation(std::function<void(ComponentId type)> closure);
        void readEncapsulation(std::function<void(ProfileId type)> closure);
        void readEncapsulation(std::function<void(ServiceId type)> closure);

        inline void readEndian() { buffer.readEndian(); }
        inline bool readBoolean() { return buffer.readBoolean(); }
        inline uint8_t readOctet()  { return buffer.readOctet(); }
        inline char8_t readChar() { return buffer.readChar(); }
        inline uint16_t readUshort() { return buffer.readUshort(); }
        inline uint32_t readUlong() { return buffer.readUlong(); }
        inline uint64_t readUlonglong() { return buffer.readUlonglong(); }
        inline uint16_t readShort() { return buffer.readShort(); }
        inline uint32_t readLong() { return buffer.readLong(); }
        inline uint64_t readLonglong() { return buffer.readLonglong(); }
        inline float readFloat() { return buffer.readFloat(); }
        inline double readDouble() { return buffer.readDouble(); }

        // WHEN DECODING AS OUT -> string|blob
        // WHEN DECODING AS IN  -> string_view|blob_view
        inline CORBA::blob readBlob() { return buffer.readBlob(); }
        inline std::string readString() { return buffer.readString(); }
        inline std::string readString(size_t length) { return buffer.readString(length); }

        inline CORBA::blob_view readBlobView() { return buffer.readBlobView(); }
        inline std::string_view readStringView() { return buffer.readStringView(); }
        inline std::string_view readStringView(size_t length) { return buffer.readStringView(length); }
        // sequence
        // value
        // object
        // reference

        std::shared_ptr<Object> readObject(std::shared_ptr<CORBA::ORB> orb = std::shared_ptr<CORBA::ORB>());
        std::shared_ptr<IOR> readReference(size_t length);
        std::shared_ptr<IOR> readReference() { return readReference(buffer.readUlong()); }
};

}  // namespace CORBA
