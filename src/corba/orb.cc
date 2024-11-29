#include "orb.hh"

#include <format>
#include <iostream>
#include <map>
#include <print>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <typeinfo>
#include <uuid/uuid.h>

#include "cdr.hh"
#include "corba.hh"
#include "giop.hh"
#include "hexdump.hh"
#include "naming.hh"
#include "net/protocol.hh"
#include "net/connection.hh"
#include "url.hh"

#define CORBA_IIOP_PORT 683
#define CORBA_IIOP_SSL_PORT 684
#define CORBA_MANAGEMENT_AGENT_PORT 1050
#define CORBA_LOC_PORT 2809

using namespace std;

namespace CORBA {

string prefix(ORB *orb) {
    string result;
    if (orb->logname) {
        result += format("ORB({}): ", orb->logname);
    }
    return result;
}

std::map<CORBA::Object *, std::function<void()>> exceptionHandler;
void installSystemExceptionHandler(std::shared_ptr<CORBA::Object> object, std::function<void()> handler) { exceptionHandler[object.get()] = handler; }
Object::~Object() {
    auto h = exceptionHandler.find(this);
    if (h != exceptionHandler.end()) {
        println("Object::~Object(): removing global exception handler");
        exceptionHandler.erase(h);
    }
}

void ORB::dump() {
    println("CORBA.CC DUMP");
    println("    CONNECTIONS: {}", connections.size());
    connections.print();
}

async<shared_ptr<Object>> ORB::stringToObject(const std::string &iorString) {
    // std::println("ORB::stringToObject(\"{}\"): enter", iorString);
    auto uri = decodeURI(iorString);
    if (std::holds_alternative<IOR>(uri)) {
        auto ior = std::get<IOR>(uri);
        auto reference = std::make_shared<IOR>(this->shared_from_this(), ior.oid, ior.host, ior.port, ior.objectKey);
        co_return dynamic_pointer_cast<Object, IOR>(reference);
    }
    if (std::holds_alternative<CorbaName>(uri)) {
        auto name = std::get<CorbaName>(uri);
        auto addr = name.addr[0];  // TODO: we only try the 1st one
        if (addr.proto == "iiop") {
            // get remote NameService (FIXME: what if it's us?)
            // std::println("ORB::stringToObject(\"{}\"): get connection to {}:{}", iorString, addr.host, addr.port);
            CORBA::detail::Connection *nameConnection = getConnection(addr.host, addr.port);
            // std::println("ORB::stringToObject(\"{}\"): got connection to {}:{}", iorString, addr.host, addr.port);
            auto it = nameConnection->stubsById.find(name.objectKey);
            NamingContextExtStub *rootNamingContext;
            if (it == nameConnection->stubsById.end()) {
                // std::println("ORB::stringToObject(\"{}\"): creating NamingContextExtStub(orb, objectKey=\"{}\", connection)", iorString, name.objectKey);
                rootNamingContext = new NamingContextExtStub(this->shared_from_this(), name.objectKey, nameConnection);
                nameConnection->stubsById[name.objectKey] = rootNamingContext;
            } else {
                // std::println("ORB::stringToObject(\"{}\"): reusing NamingContextExtStub", iorString);
                rootNamingContext = dynamic_cast<NamingContextExtStub *>(it->second);
                if (rootNamingContext == nullptr) {
                    throw runtime_error("Not a NamingContextExt");
                }
            }
            // get object from remote NameServiceExt
            // std::println("ORB::stringToObject(\"{}\"): calling resolve_str(\"{}\") on remote end", iorString, name.name);
            auto reference = co_await rootNamingContext->resolve_str(name.name);
            // std::println("ORB::stringToObject(\"{}\"): got reference", iorString);
            reference->set_ORB(this->shared_from_this());
            co_return dynamic_pointer_cast<Object, IOR>(reference);
        }
    }
    throw runtime_error(format("ORB::stringToObject(\"{}\") failed", iorString));
}

void ORB::registerProtocol(detail::Protocol *protocol) { 
    protocol->orb = this;
    protocols.push_back(protocol);
}

detail::Connection * ORB::getConnection(string host, uint16_t port) {
    // if (host == "::1" || host == "127.0.0.1") {
    //     host = "localhost";
    // }
    auto conn = connections.find(host.c_str(), port);
    if (conn) {
        println("{}ORB::getConnection(\"{}\", {}) found {}", prefix(this), host, port, conn->str());
        return conn;
    }
    // for (auto &conn : connections) {
    //     if (conn->remote.host == host && conn->remote.port == port) {
    //         if (debug) {
    //             println("ORB : Found active connection");
    //         }
    //         return conn;
    //     }
    // }
    for (auto &proto : protocols) {
        // no listen, use fake hostname and port
        if (proto->local.host.empty()) {
            // up() called without listen
            uuid_t uuid;
#ifdef _UUID_STRING_T
            uuid_string_t uuid_str;
#else
            char uuid_str[UUID_STR_LEN];
#endif
            uuid_generate_random(uuid);
            uuid_unparse_lower(uuid, uuid_str);

            proto->local.host = uuid_str;
            proto->local.port = CORBA_LOC_PORT;
        }

        if (debug) {
            if (connections.size() == 0) {
                println("{}Creating new connection to {}:{} as no others exist", prefix(this), host, port);
            } else {
                println("{}Creating new connection to {}:{}, as none found to", prefix(this), host, port);
            }
            // for (auto conn : connections) {
            //     println("ORB : active connection {}", conn->str());
            // }
        }
        auto connection = proto->connect(host.c_str(), port);
        println("{}created {}", prefix(this), connection->str());
        connections.insert(connection);
        return connection.get();
    }
    throw runtime_error(format("failed to allocate connection to {}:{}", host, port));
}

void ORB::close(detail::Connection *connection) {}

async<GIOPDecoder *> ORB::_twowayCall(Stub *stub, const char *operation, std::function<void(GIOPEncoder &)> encode) {
    // println("ORB::_twowayCall(stub, \"{}\", ...) ENTER", operation);
    if (stub->connection == nullptr) {
        throw runtime_error("ORB::_twowayCall(): the stub has no connection");
    }
    auto requestId = stub->connection->requestId.fetch_add(2);
    // printf("CONNECTION %p %s:%u -> %s:%u requestId=%u\n", static_cast<void *>(stub->connection), stub->connection->localAddress().c_str(),
    //        stub->connection->localPort(), stub->connection->remoteAddress().c_str(), stub->connection->remotePort(), stub->connection->requestId);

    GIOPEncoder encoder(stub->connection);
    auto responseExpected = true;
    encoder.encodeRequest(stub->objectKey, operation, requestId, responseExpected);
    encode(encoder);
    encoder.setGIOPHeader(MessageType::REQUEST);  // THIS IS TOTAL BOLLOCKS BECAUSE OF THE RESIZE IN IT...
    if (debug) {
        println("ORB::_twowayCall(stub, \"{}\", ...) SEND REQUEST objectKey=\"{}\", operation=\"{}\", requestId={}", operation, stub->objectKey, operation,
                requestId);
    }
    try {
        lock_guard guard(stub->connection->send_mutex);
        stub->connection->send(move(encoder.buffer._data));
    } catch (COMM_FAILURE &ex) {
        auto h = exceptionHandler.find(stub);
        if (h != exceptionHandler.end()) {
            println("found a global exception handler for the object");
            h->second();
            // TODO: the callback might drop the object's reference, which in turn should delete it from 'exceptionHandler'
        }
    }
    if (debug) {
        println("ORB::_twowayCall(stub, \"{}\", ...) SUSPEND FOR REPLY", operation);
    }
    GIOPDecoder *decoder = co_await stub->connection->interlock.suspend(requestId);
    if (debug) {
        println("ORB::_twowayCall(stub, \"{}\", ...) RESUME WITH REPLY", operation);
    }

    // move parts of this into a separate function so that it can be unit tested
    switch (decoder->replyStatus) {
        case ReplyStatus::NO_EXCEPTION:
            break;
        case ReplyStatus::USER_EXCEPTION:
            throw UserException();
            // throw runtime_error(format("CORBA User Exception from {}:{}", stub->connection->remoteAddress(), stub->connection->remotePort()));
            break;
        case ReplyStatus::SYSTEM_EXCEPTION: {
            // 0.4.3.2 ReplyBody: SystemExceptionReplyBody
            auto exceptionId = decoder->readString();
            auto minorCodeValue = decoder->readUlong();
            auto completionStatus = static_cast<CompletionStatus>(decoder->readUlong());
            if (exceptionId == "IDL:omg.org/CORBA/MARSHAL:1.0") {
                throw MARSHAL(minorCodeValue, completionStatus);
            } else if (exceptionId == "IDL:omg.org/CORBA/NO_PERMISSION:1.0") {
                throw NO_PERMISSION(minorCodeValue, completionStatus);
            } else if (exceptionId == "IDL:omg.org/CORBA/BAD_PARAM:1.0") {
                throw BAD_PARAM(minorCodeValue, completionStatus);
            } else if (exceptionId == "IDL:omg.org/CORBA/BAD_OPERATION:1.0") {
                throw BAD_OPERATION(minorCodeValue, completionStatus);
            } else if (exceptionId == "IDL:omg.org/CORBA/OBJECT_NOT_EXIST:1.0") {
                throw OBJECT_NOT_EXIST(minorCodeValue, completionStatus);
            } else if (exceptionId == "IDL:omg.org/CORBA/TRANSIENT:1.0") {
                throw TRANSIENT(minorCodeValue, completionStatus);
            } else if (exceptionId == "IDL:omg.org/CORBA/OBJECT_ADAPTER:1.0") {
                throw OBJECT_ADAPTER(minorCodeValue, completionStatus);
            } else if (exceptionId == "IDL:omg.org/CORBA/REBIND:1.0") {
                throw REBIND(minorCodeValue, completionStatus);
            } else if (exceptionId == "IDL:omg.org/CORBA/NO_IMPLEMENT:1.0") {
                throw NO_IMPLEMENT(minorCodeValue, completionStatus);
            } else if (exceptionId == "IDL:omg.org/CORBA/COMM_FAILURE:1.0") {
                throw COMM_FAILURE(minorCodeValue, completionStatus);
            } else if (exceptionId == "IDL:omg.org/CORBA/INITIALIZE:1.0") {
                throw INITIALIZE(minorCodeValue, completionStatus);
            } else if (exceptionId == "IDL:mark13.org/CORBA/GENERIC:1.0") {
                throw runtime_error(
                    format("Remote CORBA exception from {}: {}", stub->connection->str(), decoder->readString()));
            } else {
                throw runtime_error(
                    format("CORBA System Exception {} from {}", exceptionId, stub->connection->str()));
            }
        } break;
        default:
            throw runtime_error(format("ReplyStatusType {} is not supported", (unsigned)decoder->replyStatus));
    }
    if (debug) {
        println("ORB::_twowayCall(stub, \"{}\", ...) RETURN", operation);
    }
    co_return decoder;
}

void ORB::onewayCall(Stub *stub, const char *operation, std::function<void(GIOPEncoder &)> encode) {
    if (stub->connection == nullptr) {
        throw runtime_error("ORB::onewayCall(): the stub has no connection");
    }
    auto requestId = stub->connection->requestId.fetch_add(2);
    GIOPEncoder encoder(stub->connection);
    auto responseExpected = false;
    encoder.encodeRequest(stub->objectKey, operation, requestId, responseExpected);
    encode(encoder);
    encoder.setGIOPHeader(MessageType::REQUEST);

    try {
        lock_guard guard(stub->connection->send_mutex);
        stub->connection->send(move(encoder.buffer._data));
    } catch (COMM_FAILURE &ex) {
        auto h = exceptionHandler.find(stub);
        if (h != exceptionHandler.end()) {
            println("found a global exception handler for the object");
            h->second();
            // TODO: the callback might drop the object's reference, which in turn should delete it from 'exceptionHandler'
        }
    }
}

blob_view ORB::registerServant(Skeleton *servant) {
    // println("ORB::registerServant(servant)");
    return registerServant(servant, format("OID:{:x}", ++servantIdCounter));
}

blob_view ORB::registerServant(Skeleton *servant, const string &objectKey) {
    // println("ORB::registerServant(servant, \"{}\")", objectKey);
    auto pos = servants.emplace(std::make_pair(blob(objectKey), servant));
    servants[blob(objectKey)] = servant;
    return blob_view(std::get<0>(pos)->first);
}

void ORB::bind(const std::string &id, std::shared_ptr<CORBA::Skeleton> const obj) {
    if (namingService == nullptr) {
        // println("ORB::bind(\"{}\"): CREATING NameService", id);
        namingService = new NamingContextExtImpl(this->shared_from_this(), "NameService");
        servants["NameService"] = namingService;
    }
    namingService->bind(id, obj);
}

void ORB::socketRcvd(detail::Connection *connection, const void *buffer, size_t size) {
    if (debug) {
        println("{}socketRcvd(connection={}, buffer, size={})", prefix(this), connection->str(), size);
        // hexdump(buffer, size);
    }
    if (size == 0) {
        return;
    }
    CDRDecoder data((const char *)buffer, size);
    GIOPDecoder decoder(data);
    decoder.connection = connection;
    auto type = decoder.scanGIOPHeader();
    switch (type) {
        case MessageType::REQUEST: {
            // TODO: move this into a method
            auto request = decoder.scanRequestHeader();
            if (debug) {
                cout << "REQUEST(requestId=" << request->requestId << ", objectKey=" << request->objectKey << ", \"" << request->operation << "\")" << endl;
            }
            auto servant = servants.find(request->objectKey);  // FIXME: avoid string copy
            if (servant == servants.end()) {
                println("NO SERVANT FOUND");
                if (request->responseExpected) {
                    CORBA::GIOPEncoder encoder(connection);
                    encoder.majorVersion = decoder.majorVersion;
                    encoder.minorVersion = decoder.minorVersion;
                    encoder.skipReplyHeader();

                    encoder.writeString("IDL:omg.org/CORBA/OBJECT_NOT_EXIST:1.0");
                    encoder.writeUlong(0x4f4d0001);  // Attempt to pass an unactivated (unregistered) value as an object reference.
                    encoder.writeUlong(NO);          // completionStatus

                    auto length = encoder.buffer.offset;
                    encoder.setGIOPHeader(MessageType::REPLY);
                    encoder.setReplyHeader(request->requestId, ReplyStatus::SYSTEM_EXCEPTION);
                    connection->send(move(encoder.buffer._data));
                }
                return;
            }
            // NOTE: i can kill the server, client keeps running (will propably reconnect on demand)

            if (request->operation == "_is_a") {
                println("OPERATION _is_a()");
                auto repositoryId = decoder.readStringView();
                CORBA::GIOPEncoder encoder(connection);
                encoder.majorVersion = decoder.majorVersion;
                encoder.minorVersion = decoder.minorVersion;
                encoder.skipReplyHeader();

                auto result = (repositoryId.compare(servant->second->repository_id()) == 0);
                println("    want \"{}\",\n    have \"{}\" -> {}", repositoryId, servant->second->repository_id(), result);
                encoder.writeBoolean(repositoryId == servant->second->repository_id());

                auto length = encoder.buffer.offset;
                encoder.setGIOPHeader(MessageType::REPLY);
                encoder.setReplyHeader(request->requestId, ReplyStatus::NO_EXCEPTION);

                // hexdump(encoder.buffer.data(), length);

                connection->send(move(encoder.buffer._data));

                return;
            }

            try {
                auto encoder = make_shared<CORBA::GIOPEncoder>(connection);
                encoder->majorVersion = decoder.majorVersion;
                encoder->minorVersion = decoder.minorVersion;
                encoder->skipReplyHeader();
                bool responseExpected = request->responseExpected;
                uint32_t requestId = request->requestId;

                // move parts of this into a separate function so that it can be unit tested
                // std::cerr << "CALL SERVANT" << std::endl;
                servant->second->_call(request->operation, decoder, *encoder)
                    .thenOrCatch(
                        [this, encoder, connection, responseExpected, requestId] {  // FIXME: the references objects won't be available
                            // println("SERVANT RETURNED");
                            if (responseExpected) {
                                // println("SERVANT WANTS RESPONSE");
                                auto length = encoder->buffer.offset;
                                encoder->setGIOPHeader(MessageType::REPLY);
                                encoder->setReplyHeader(requestId, ReplyStatus::NO_EXCEPTION);
                                println("{}send REPLY via connection {}", prefix(this), connection->str());
                                // hexdump(encoder->buffer.data(), length);
                                connection->send(move(encoder->buffer._data));
                            }
                        },
                        [&](std::exception_ptr eptr) {  // FIXME: the references objects won't be available
                            try {
                                // std::rethrow_exception(ex);
                                std::rethrow_exception(eptr);
                            } catch (CORBA::UserException &ex) {
                                println("CORBA::UserException while calling local servant {}::{}(...): {}", 
                                    servant->second->repository_id(), 
                                    request->operation,
                                    ex.what()
                                );
                                if (responseExpected) {
                                    auto length = encoder->buffer.offset;
                                    encoder->setGIOPHeader(MessageType::REPLY);
                                    encoder->setReplyHeader(requestId, ReplyStatus::USER_EXCEPTION);
                                    connection->send(move(encoder->buffer._data));
                                }
                            } catch (CORBA::SystemException &error) {
                                println("{} while calling local servant {}::{}(...): {}",
                                    error._rep_id(),
                                    servant->second->repository_id(), 
                                    request->operation,
                                    error.what()
                                );
                                if (responseExpected) {
                                    encoder->writeString(error._rep_id());
                                    encoder->writeUlong(error.minor);
                                    encoder->writeUlong(error.completed);
                                    auto length = encoder->buffer.offset;
                                    encoder->setGIOPHeader(MessageType::REPLY);
                                    encoder->setReplyHeader(requestId, ReplyStatus::SYSTEM_EXCEPTION);
                                    connection->send(move(encoder->buffer._data));
                                }
                            } catch (std::exception &ex) {
                                println("std::exception while calling local servant {}::{}(...): {}", 
                                    servant->second->repository_id(), 
                                    request->operation,
                                    ex.what()
                                );
                                if (responseExpected) {
                                    encoder->writeString("IDL:mark13.org/CORBA/GENERIC:1.0");
                                    encoder->writeUlong(0);
                                    encoder->writeUlong(0);
                                    encoder->writeString(format("IDL:{}:1.0: {}", typeid(ex).name(), ex.what()));
                                    auto length = encoder->buffer.offset;
                                    encoder->setGIOPHeader(MessageType::REPLY);
                                    encoder->setReplyHeader(requestId, ReplyStatus::SYSTEM_EXCEPTION);
                                    connection->send(move(encoder->buffer._data));
                                }
                            } catch (...) {
                                println("SERVANT THREW EXCEPTION");
                            }
                        });
            } catch (std::out_of_range &e) {
                if (request->responseExpected) {
                    // send reply
                }
                cerr << "OUT OF RANGE: " << e.what() << endl;
            } catch (std::exception &e) {
                if (request->responseExpected) {
                    // send reply
                }
                cerr << "ORB::socketRcvd: EXCEPTION: " << e.what() << endl;
            }
        } break;

        case MessageType::REPLY: {
            auto _data = decoder.scanReplyHeader();
            if (debug) {
                println("ORB::socketRcvd(): REPLY, resume requestId {}", _data->requestId);
            }
            try {
                connection->interlock.resume(_data->requestId, &decoder);
            } catch (...) {
                println("ORB::socketRcvd(): unexpected reply to requestId {}", _data->requestId);
            }
            break;
        } break;

        case MessageType::LOCATE_REQUEST: {
            auto _data = decoder.scanLocateRequest();  // actuall
            auto servant = servants.find(_data->objectKey);
            GIOPEncoder encoder(connection);
            encoder.majorVersion = decoder.majorVersion;
            encoder.minorVersion = decoder.minorVersion;

            encoder.encodeLocateReply(_data->requestId, servant != servants.end() ? LocateStatusType::OBJECT_HERE : LocateStatusType::UNKNOWN_OBJECT);
            encoder.setGIOPHeader(MessageType::LOCATE_REPLY);
            connection->send(move(encoder.buffer._data));
            delete _data;
        } break;

        case MessageType::MESSAGE_ERROR: {
            println("ORB::socketRcvd(): RECEIVED MESSAGE ERROR");
        } break;
        default:
            cout << "ORB::socketRcvd(): GOT YET UNIMPLEMENTED REQUEST OF TYPE " << static_cast<unsigned>(type) << endl;
            break;
    }
}

}  // namespace CORBA
