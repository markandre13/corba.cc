#include "giop.hh"

#include <array>
#include <format>
#include <iostream>
#include <stdexcept>

#include "blob.hh"
#include "corba.hh"
#include "hexdump.hh"
#include "orb.hh"
#include "protocol.hh"

using namespace std;

namespace CORBA {

void GIOPEncoder::writeObject(const CORBA::Object* object) {
    // cerr << "GIOPEncoder::object(...)" << endl;
    if (object == nullptr) {
        buffer.writeUlong(0);
        return;
    }
    auto stub = dynamic_cast<const Stub*>(object);
    if (stub != nullptr) {
        throw runtime_error("Can not serialize stub yet.");
    }
    auto skeleton = dynamic_cast<const Skeleton*>(object);
    if (skeleton != nullptr) {
        writeReference(object);
        return;
    }
    auto reference = dynamic_cast<const IOR*>(object);
    if (reference != nullptr) {
        throw runtime_error("Can not serialize object reference yet.");
        // reference(object);
        return;
    }
    throw runtime_error("Can not serialize value type yet.");
}

// Interoperable Object Reference (IOR)
void GIOPEncoder::writeReference(const Object* object) {
    // cerr << "GIOPEncoder::reference(...) ENTER" << endl;
    // const className = (object.constructor as any)._idlClassName()
    auto className = object->repository_id();
    // if (className == nullptr) {
    //     cerr << "GIOPEncoder::reference(...) [1]" << endl;
    //     throw runtime_error("GIOPEncoder::reference(): _idlClassName() must not return nullptr");
    // }

    // type id
    writeString(className);

    // tagged profile sequence
    writeUlong(1);  // profileCount

    // profile id
    // 9.7.2 IIOP IOR Profiles
    writeUlong(0);  // IOR.TAG.IOR.INTERNET_IOP
    reserveSize();
    writeEndian();
    writeOctet(majorVersion);
    writeOctet(minorVersion);

    if (connection == nullptr) {
        cerr << "GIOPEncoder::reference(...) [2]" << endl;
        throw runtime_error("GIOPEncoder::reference(...): the encoder has no connection and can not be reached over the network");
    }
    writeString(connection->localAddress());
    writeUshort(connection->localPort());
    writeBlob(object->get_object_key());

    // IIOP >= 1.1: components
    if (majorVersion != 1 || minorVersion != 0) {
        writeUlong(1);                                        // component count = 1
        writeEncapsulation(ComponentId::ORB_TYPE, [this]() {  // 0:  TAG_ORB_TYPE (3.4 P 2, 7.6.6.1)
            writeUlong(0x4d313300);                           // "M13\0" as ORB Type ID for corba.js
        });
    }
    fillInSize();
    // cerr << "GIOPEncoder::reference(...) LEAVE" << endl;
}

void GIOPEncoder::writeEncapsulation(ComponentId type, std::function<void()> closure) {
    writeUlong(static_cast<uint32_t>(type));
    reserveSize();
    writeEndian();
    closure();
    fillInSize();
}
void GIOPEncoder::writeEncapsulation(ProfileId type, std::function<void()> closure) {
    writeUlong(static_cast<uint32_t>(type));
    reserveSize();
    writeEndian();
    closure();
    fillInSize();
}
void GIOPEncoder::writeEncapsulation(ServiceId type, std::function<void()> closure) {
    writeUlong(static_cast<uint32_t>(type));
    reserveSize();
    writeEndian();
    closure();
    fillInSize();
}

void GIOPEncoder::skipGIOPHeader() { buffer.offset = 10; }
void GIOPEncoder::skipReplyHeader() {
    buffer.offset = 24;  // this does not work!!! anymore with having a variable length service context!!!
}
void GIOPEncoder::setGIOPHeader(MessageType type) {
    buffer.reserve();
    auto offset = buffer.offset;
    buffer.offset = 0;
    writeOctet('G');
    writeOctet('I');
    writeOctet('O');
    writeOctet('P');
    writeOctet(majorVersion);
    writeOctet(minorVersion);
    writeEndian();
    writeOctet(static_cast<uint8_t>(type));
    writeUlong(offset - 12);
    buffer.offset = offset;
}
void GIOPEncoder::setReplyHeader(uint32_t requestId, ReplyStatus replyStatus) {
    skipGIOPHeader();
    // fixme: create and use version methods like isVersionLessThan(1,2) or isVersionVersionGreaterEqual(1,2)
    if (majorVersion == 1 && minorVersion < 2) {
        // this.serviceContext()
        writeUlong(0);  // skipReplyHeader needs a fixed size service context
    }
    writeUlong(requestId);
    writeUlong(static_cast<uint32_t>(replyStatus));
    if (majorVersion == 1 && minorVersion >= 2) {
        // this.serviceContext();
        writeUlong(0);  // skipReplyHeader needs a fixed size service context
    }
}

void GIOPEncoder::encodeRequest(const CORBA::blob& objectKey, const std::string& operation, uint32_t requestId, bool responseExpected) {
    skipGIOPHeader();

    if (majorVersion == 1 && minorVersion <= 1) {
        writeServiceContext();
    }
    writeUlong(requestId);
    if (majorVersion == 1 && minorVersion <= 1) {
        writeOctet(responseExpected ? 1 : 0);
    } else {
        writeOctet(responseExpected ? 3 : 0);
    }
    buffer.offset += 3;

    if (majorVersion == 1 && minorVersion <= 1) {
        this->writeBlob(objectKey);
    } else {
        writeUshort(static_cast<uint16_t>(AddressingDisposition::KEY_ADDR));
        this->writeBlob(objectKey);
    }

    writeString(operation);
    if (majorVersion == 1 && minorVersion <= 1) {
        writeUlong(0);  // Requesting Principal length
    } else {
        writeServiceContext();
        buffer.align8();  // alignAndReserve(8);
    }
}

void GIOPEncoder::writeServiceContext() {
    // TODO: remove this, this happens only in tests
    if (connection == nullptr) {
        writeUlong(0);
        return;
    }

    auto count = 0;
    auto offset = buffer.offset;
    writeUlong(0);

    // CORBA 3.4 Part 2, 9.8.1 Bi-directional IIOP Service Context
    // TODO: send listen point only once per connection
    if (!connection->didSendBiDirIIOP) {
        ++count;
        connection->didSendBiDirIIOP = true;
        writeEncapsulation(ServiceId::BI_DIR_IIOP, [this] {
            writeUlong(1); // number of listen points
            writeString(connection->localAddress());
            writeUshort(connection->localPort());
        });
    }

    // if (connection.orb.outgoingAuthenticator) {
    //     initialToken = connection.orb.outgoingAuthenticator(this.connection)
    //     if (initialToken) {
    //         ++count
    //         this.establishSecurityContext(initialToken);
    //     }
    // }

    /*
    this.beginEncapsulation(ServiceId.CodeSets)
    // this.ulong(0x00010001) // ISO-8859-1
    this.ulong(0x05010001) // charset_id : UTF-8
    this.ulong(0x00010109) // wcharset_id: UTF-16
    this.endEncapsulation()
    */

   auto offset0 = buffer.offset;
   buffer.offset = offset;
   writeUlong(count);
   buffer.offset = offset0;
}

////////////////////////////////////////////

MessageType GIOPDecoder::scanGIOPHeader() {
    auto header = reinterpret_cast<const GIOPHeader*>(buffer.data());
    if (memcmp(header->id, "GIOP", 4) != 0) {
        throw std::runtime_error("Missing GIOP Header");
    }
    buffer.setOffset(4);
    majorVersion = readOctet();
    minorVersion = readOctet();
    auto flags = readOctet();
    buffer.setLittleEndian(flags & 1);
    m_type = static_cast<MessageType>(readOctet());
    m_length = readUlong();
    return m_type;
}

const RequestHeader* GIOPDecoder::scanRequestHeader() {
    // auto header = reinterpret_cast<const RequestHeader*>(buffer.data() + offset);
    // make_unique<RequestHeader>();
    if (majorVersion == 1 && minorVersion <= 1) {
        readServiceContext();
        cout << "SERVICE CONTEXT" << endl;
    }
    auto header = new RequestHeader();
    header->requestId = readUlong();
    auto responseFlags = readOctet();
    if (majorVersion == 1 && minorVersion <= 1) {
        header->responseExpected = responseFlags != 0;
    } else {
        switch (responseFlags) {
            case 0:  // SyncScope.NONE, WITH_TRANSPORT
                header->responseExpected = false;
                break;
            case 1:  // WITH_SERVER
                break;
            case 2:
                break;
            case 3:  // WITH_TARGET
                header->responseExpected = true;
                break;
        }
    }
    buffer.skip(3);  // RequestReserved

    if (majorVersion == 1 && minorVersion <= 1) {
        header->objectKey = readBlobView();
    } else {
        auto addressingDisposition = static_cast<AddressingDisposition>(readUshort());
        switch (addressingDisposition) {
            case AddressingDisposition::KEY_ADDR:
                header->objectKey = readBlobView();
                break;
            case AddressingDisposition::PROFILE_ADDR:
            case AddressingDisposition::REFERENCE_ADDR:
                throw runtime_error("Unsupported AddressingDisposition.");
            default:
                throw runtime_error("Unknown AddressingDisposition.");
        }
    }
    // cout << "REQUEST objectKey size = " << header->objectKey.length << endl;
    header->operation = readStringView();

    if (majorVersion == 1 && minorVersion <= 1) {
        auto requestingPrincipalLength = readUlong();
        // FIXME: this.offset += requestingPrincipalLength???
    } else {
        readServiceContext();
        // header->serviceContext = serviceContext();
        buffer.align8();
    }
    return header;
}

const LocateRequest* GIOPDecoder::scanLocateRequest() {
    this->requestId = readUlong();
    CORBA::blob_view objectKey;
    if (majorVersion == 1 && minorVersion <= 1) {
        objectKey = readBlobView();
    } else {
        auto addressingDisposition = static_cast<AddressingDisposition>(readUshort());
        switch (addressingDisposition) {
            case AddressingDisposition::KEY_ADDR:
                objectKey = readBlobView();
                break;
            case AddressingDisposition::PROFILE_ADDR:
            case AddressingDisposition::REFERENCE_ADDR:
                throw runtime_error("Unsupported AddressingDisposition.");
            default:
                throw runtime_error("Unknown AddressingDisposition.");
        }
    }
    return new LocateRequest(this->requestId, objectKey);  // unique_ptr
}

unique_ptr<ReplyHeader> GIOPDecoder::scanReplyHeader() {
    if (majorVersion == 1 && minorVersion <= 1) {
        readServiceContext();
    }
    this->requestId = readUlong();
    this->replyStatus = static_cast<ReplyStatus>(readUlong());
    if (majorVersion == 1 && minorVersion >= 2) {
        readServiceContext();
    }
    return make_unique<ReplyHeader>(this->requestId, this->replyStatus);
}

void GIOPDecoder::readServiceContext() {
    auto serviceContextListLength = readUlong();
    for (size_t i = 0; i < serviceContextListLength; ++i) {
        readEncapsulation([this](ServiceId serviceId) {
            switch (serviceId) {
                case ServiceId::CodeSets:
                    // std::cout << "ServiceContext CodeSets" << std::endl;
                    break;
                case ServiceId::BI_DIR_IIOP: {  
                    cout << "ServiceContext BI_DIR_IIOP" << endl;
                    auto count = readUlong();
                    for(auto i = 0; i<count; ++i) {
                        auto host = readStringView();
                        auto port = readUshort();
                        cout << "    " << host << ":" << port << endl;
                        connection->addPeer(host, port);
                    }
                } break;
                case ServiceId::SecurityAttributeService:
                    cout << "ServiceContext SecurityAttributeService" << endl;
                    break;
                default:
                    cout << "ServiceContext " << static_cast<unsigned>(serviceId) << endl;
            }
        });
    }
}

void GIOPDecoder::readEncapsulation(std::function<void(ComponentId type)> closure) {
    auto type = static_cast<ComponentId>(readUlong());
    auto size = readUlong();
    auto nextOffset = buffer.getOffset() + size;
    auto lastEndian = buffer._endian;
    auto flags = readOctet();
    buffer.setLittleEndian(flags & 1);

    closure(type);

    buffer._endian = lastEndian;
    buffer.setOffset(nextOffset);
}
void GIOPDecoder::readEncapsulation(std::function<void(ProfileId type)> closure) {
    auto type = static_cast<ProfileId>(readUlong());
    auto size = readUlong();
    auto nextOffset = buffer.getOffset() + size;
    auto lastEndian = buffer._endian;
    auto flags = readOctet();
    buffer.setLittleEndian(flags & 1);

    closure(type);

    buffer._endian = lastEndian;
    buffer.setOffset(nextOffset);
}
void GIOPDecoder::readEncapsulation(std::function<void(ServiceId type)> closure) {
    auto type = static_cast<ServiceId>(readUlong());
    auto size = readUlong();
    auto nextOffset = buffer.getOffset() + size;
    auto lastEndian = buffer._endian;
    auto flags = readOctet();
    buffer.setLittleEndian(flags & 1);

    closure(type);

    buffer._endian = lastEndian;
    buffer.setOffset(nextOffset);
}

std::shared_ptr<Object> GIOPDecoder::readObject(std::shared_ptr<CORBA::ORB> orb) {  // const string typeInfo, bool isValue = false) {
    auto code = readUlong();
    // auto objectOffset = buffer.m_offset - 4;

    if (code == 0) {
        return std::shared_ptr<Object>();
    }

    if ((code & 0xffffff00) == 0x7fffff00) {
        throw runtime_error("GIOPDecoder::object(): value types are not implemented yet");
    }

    if (code == 0xffffffff) {
        throw runtime_error("GIOPDecoder::object(): pointers are not implemented yet");
    }

    if (code < 0x7fffff00) {
        if (!orb) {
            throw runtime_error("GIOPDecoder::object(orb): orb must not be null");
        }
        auto ref = readReference(code);
        // cerr << "GOT IOR " << ref->oid << " " << ref->objectKey << endl;
        return make_shared<IOR>(orb, ref->oid, ref->host, ref->port, ref->get_object_key());
    }
    throw runtime_error(format("GIOPDecoder: Unsupported value with CORBA tag {:#x}", code));
}

struct ORBTypeName {
        uint32_t from;
        uint32_t to;
        const char* name;
};

static array<ORBTypeName, 35> orbTypeNames{{{0x48500000, 0x4850000f, "Hewlett Packard"},
                                            {0x49424d00, 0x49424d0f, "IBM"},
                                            {0x494c5500, 0x494c55ff, "Xerox"},
                                            {0x49534900, 0x4953490f, "AdNovum Informatik AG"},
                                            {0x56495300, 0x5649530f, "Borland (VisiBroker)"},
                                            {0x4f495300, 0x4f4953ff, "Objective Interface Systems"},
                                            {0x46420000, 0x4642000f, "FloorBoard Software"},
                                            {0x4E4E4E56, 0x4E4E4E56, "Rogue Wave"},
                                            {0x4E550000, 0x4E55000f, "Nihon Unisys, Ltd"},
                                            {0x4A424B52, 0x4A424B52, "SilverStream Software"},
                                            {0x54414f00, 0x54414f00, "Center for Distributed Object Computing, Washington University"},
                                            {0x4C434200, 0x4C43420F, "2AB"},
                                            {0x41505831, 0x41505831, "Informatik 4, Univ. of Erlangen-Nuernberg"},
                                            {0x4f425400, 0x4f425400, "ORBit"},
                                            {0x47534900, 0x4753490f, "GemStone Systems, Inc."},
                                            {0x464a0000, 0x464a000f, "Fujitsu Limited"},
                                            {0x4E534440, 0x4E53444F, "Compaq Computer"},
                                            {0x4f425f00, 0x4f425f0f, "TIBCO"},
                                            {0x4f414b00, 0x4f414b0f, "Camros Corporation"},
                                            {0x41545400, 0x4154540f, "AT&T Laboratories, Cambridge (OmniORB)"},
                                            {0x4f4f4300, 0x4f4f430f, "IONA Technologies"},
                                            {0x4e454300, 0x4e454303, "NEC Corporation"},
                                            {0x424c5500, 0x424c550f, "Berry Software"},
                                            {0x56495400, 0x564954ff, "Vitra"},
                                            {0x444f4700, 0x444f47ff, "Exoffice Technologies"},
                                            {0xcb0e0000, 0xcb0e00ff, "Chicago Board of Exchange (CBOE)"},
                                            {0x4A414300, 0x4A41430f, "FU Berlin Institut für Informatik (JAC)"},
                                            {0x58545240, 0x5854524F, "Xtradyne Technologies AG"},
                                            {0x54475800, 0x54475803, "Top Graph'X"},
                                            {0x41646100, 0x41646103, "AdaOS project"},
                                            {0x4e4f4b00, 0x4e4f4bff, "Nokia"},
                                            {0x53414E00, 0x53414E0f, "Sankhya Technologies Private Limited, India"},
                                            {0x414E4400, 0x414E440f, "Androsoft GmbH"},
                                            {0x42424300, 0x4242430f, "Bionic Buffalo Corporation"},
                                            {0x4d313300, 0x4d313300, "corba.js"}}};

shared_ptr<IOR> GIOPDecoder::readReference(size_t length) {
    auto oid = readString(length);
    std::string host;
    uint16_t port;
    CORBA::blob objectKey;

    auto profileCount = readUlong();
    // console.log(`oid: '${oid}', tag count=${tagCount}`)
    for (uint32_t i = 0; i < profileCount; ++i) {
        readEncapsulation([&](ProfileId profileId) {
            switch (profileId) {
                // CORBA 3.3 Part 2: 9.7.2 IIOP IOR Profiles
                case ProfileId::TAG_INTERNET_IOP: {
                    // console.log(`Internet IOP Component, length=${profileLength}`)
                    auto iiopMajorVersion = readOctet();
                    auto iiopMinorVersion = readOctet();
                    // if (iiopMajorVersion != 1 || iiopMinorVersion > 1) {
                    //     throw runtime_error(format("Unsupported IIOP version {}.{}. Must be 1.1+", iiopMajorVersion, iiopMinorVersion));
                    // }
                    host = readString();
                    port = readUshort();
                    objectKey = readBlob();
                    // FIXME: use utility function to compare version!!! better use hex: version >= 0x0101
                    // if (iiopMajorVersion == 1 && iiopMinorVersion != 0) {
                    //     readEncapsulation([&](ComponentId componentId) {
                    //         switch (componentId) {
                    //             case ComponentId::ORB_TYPE: {
                    //                 auto typeCount = readUlong();
                    //                 for (uint32_t j = 0; j < typeCount; ++j) {
                    //                     auto orbType = readUlong();
                    //                     const char* name = nullptr;
                    //                     for (const auto& orbTypeName : orbTypeNames) {
                    //                         if (orbTypeName.from <= orbType && orbType <= orbTypeName.to) {
                    //                             name = orbTypeName.name;
                    //                             break;
                    //                         }
                    //                     }
                    //                 }
                    //                 println("GOT ORB_TYPE");
                    //             } break;
                    //             case ComponentId::CODE_SETS:
                    //                 // Corba 3.4, Part 2, 7.10.2.4 CodeSet Component of IOR Multi-Component Profile
                    //                 // console.log(`IOR: component[${i}] = CODE_SETS`)
                    //                 println("GOT CODE_SETS");
                    //                 break;
                    //             case ComponentId::POLICIES: // there are some about reconnection...
                    //                 println("GOT POLICIES");
                    //                 // console.log(`IOR: component[${i}] = POLICIES`)
                    //                 break;
                    //             default:
                    //                 // console.log(`IOR: component[${i}] = ${id} (0x${id.toString(16)})`)
                    //         };
                    //     });
                    // }
                } break;
                default: {
                    // auto id = static_cast<unsigned>(profileId);
                    // cerr << format("IOR: Unhandled profile id={} {:x}", id, id) << endl;
                }
            }
        });
    }
    return make_shared<IOR>(nullptr, oid, host, port, objectKey);
}

}  // namespace CORBA