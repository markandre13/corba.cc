#include "orb.hh"

#include <format>
#include <iostream>
#include <map>
#include <print>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

#include "cdr.hh"
#include "corba.hh"
#include "giop.hh"
#include "hexdump.hh"
#include "protocol.hh"
#include "url.hh"

using namespace std;

namespace CORBA {

class NamingContextExtImpl : public Skeleton {
        std::map<std::string, std::shared_ptr<Object>> name2Object;

    public:
        NamingContextExtImpl(std::shared_ptr<CORBA::ORB> orb, const std::string &objectKey) : Skeleton(orb, objectKey) {}
        virtual std::string_view repository_id() const override;

        void bind(const std::string &name, std::shared_ptr<Object> servant) {
            if (name2Object.contains(name)) {
                throw runtime_error(format("name \"{}\" is already bound to object", name));
            }
            name2Object[name] = servant;
        }

        std::shared_ptr<Object> resolve(const std::string &name) {
            // println("NamingContextImpl.resolve(\"{}\")", name);
            auto servant = name2Object.find(name);
            if (servant == name2Object.end()) {
                println("NamingContextExtImpl::resolve(\"{}\"): name is not bound to an object", name);
                hexdump(name);
                throw runtime_error(format("NamingContextExtImpl::resolve(\"{}\"): name is not bound to an object", name));
            }
            return servant->second;
        }

    protected:
        void _orb_resolve(GIOPDecoder &decoder, GIOPEncoder &encoder) {
            auto entries = decoder.readUlong();
            auto name = decoder.readString();
            auto key = decoder.readString();
            if (entries != 1 && !key.empty()) {
                cerr << "warning: resolve got " << entries << " (expected 1) and/or key is \"" << key << "\" (expected \"\")" << endl;
            }
            std::shared_ptr<Object> result = resolve(name);  // FIXME: we don't want to copy the string
            encoder.writeObject(result.get());
        }

        void _orb_resolve_str(GIOPDecoder &decoder, GIOPEncoder &encoder) {
            auto name = decoder.readString();
            auto result = resolve(name);
            encoder.writeObject(result.get());
        }

        CORBA::async<> _call(const std::string_view &operation, GIOPDecoder &decoder, GIOPEncoder &encoder) override {
            // cerr << "NamingContextExtImpl::_call(" << operation << ", ...)"
            // << endl;
            if (operation == "resolve") {
                _orb_resolve(decoder, encoder);
                co_return;
            }
            if (operation == "resolve_str") {
                _orb_resolve_str(decoder, encoder);
                co_return;
            }
            // TODO: throw a BAD_OPERATION system exception here
            throw runtime_error(std::format("bad operation: '{}' does not exist", operation));
        }
};

class NamingContextExtStub : public Stub {
    public:
        NamingContextExtStub(std::shared_ptr<CORBA::ORB> orb, const string &objectKey, detail::Connection *connection)
            : Stub(orb, blob_view(objectKey), connection) {}
        std::string_view repository_id() const override;

        // static narrow(object: any): NamingContextExtStub {
        //     if (object instanceof NamingContextExtStub)
        //         return object as NamingContextExtStub
        //     throw Error("NamingContextExt.narrow() failed")
        // }

        // TODO: the argument doesn't match the one in the IDL but for now it's
        // good enough
        async<shared_ptr<IOR>> resolve_str(const string &name) {
            return get_ORB()->twowayCall<shared_ptr<IOR>>(
                this, "resolve_str",
                [name](GIOPEncoder &encoder) {
                    encoder.writeString(name);
                },
                [](GIOPDecoder &decoder) {
                    return decoder.readReference();
                });
        }
};

std::string_view _rid("omg.org/CosNaming/NamingContextExt");
std::string_view NamingContextExtImpl::repository_id() const { return _rid; }
std::string_view NamingContextExtStub::repository_id() const { return _rid; }

Object::~Object() {}

ORB::ORB() {}

async<shared_ptr<Object>> ORB::stringToObject(const std::string &iorString) {
    // std::println("ORB::stringToObject(\"{}\"): enter", iorString);
    auto uri = decodeURI(iorString);
    if (std::holds_alternative<IOR>(uri)) {
        // return iorToObject(std::get<IOR>(uri));
    }
    if (std::holds_alternative<CorbaName>(uri)) {
        auto name = std::get<CorbaName>(uri);
        auto addr = name.addr[0];  // TODO: we only try the 1st one
        if (addr.proto == "iiop") {
            // get remote NameService (FIXME: what if it's us?)
            // std::println("ORB::stringToObject(\"{}\"): get connection to {}:{}", iorString, addr.host, addr.port);
            CORBA::detail::Connection *nameConnection = co_await getConnection(addr.host, addr.port);
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

async<detail::Connection *> ORB::getConnection(string host, uint16_t port) {
    println("ORB::getConnection(\"{}\", {}) <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<", host, port);

    if (host == "::1" || host == "127.0.0.1") {
        host = "localhost";
    }
    for (auto conn : connections) {
        if (conn->remoteAddress() == host && conn->remotePort() == port) {
            println("ORB : Found active connection");
            co_return conn;
        }
    }
    for (auto proto : protocols) {
        if (debug) {
            if (connections.size() == 0) {
                println("ORB : Creating new connection to {}:{} as no others exist", host, port);
            } else {
                println("ORB : Creating new connection to {}:{}, as none found to", host, port);
            }
            for (auto conn : connections) {
                println("ORB : active connection {}:{}", conn->remoteAddress(), conn->remotePort());
            }
        }
        CORBA::detail::Connection *connection = co_await proto->connect(this, host, port);
        if (connection == nullptr) {
            throw runtime_error(format("failed to get connection to {}:{}", host, port));
        }
        println("CREATED CONNECTION FROM {}:{} TO {}:{}", connection->localAddress(), connection->remotePort(), connection->remoteAddress(), connection->remotePort());
        connections.push_back(connection);
        co_return connection;
    }
    throw runtime_error(format("failed to allocate connection to {}:{}", host, port));
}

async<GIOPDecoder *> ORB::_twowayCall(Stub *stub, const char *operation, std::function<void(GIOPEncoder &)> encode) {
    // println("ORB::_twowayCall(stub, \"{}\", ...) ENTER", operation);
    if (stub->connection == nullptr) {
        throw runtime_error("ORB::_twowayCall(): the stub has no connection");
    }
    auto requestId = stub->connection->requestId;
    stub->connection->requestId += 2;
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
    stub->connection->send((void *)encoder.buffer.data(), encoder.buffer.offset);
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
            } else if (exceptionId == "IDL:mark13.org/CORBA/GENERIC:1.0") {
                throw runtime_error(
                    format("Remote CORBA exception from {}:{}: {}", stub->connection->remoteAddress(), stub->connection->remotePort(), decoder->readString()));
            } else {
                throw runtime_error(
                    format("CORBA System Exception {} from {}:{}", exceptionId, stub->connection->remoteAddress(), stub->connection->remotePort()));
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
    auto requestId = stub->connection->requestId;
    stub->connection->requestId += 2;
    GIOPEncoder encoder(stub->connection);
    auto responseExpected = true;
    encoder.encodeRequest(stub->objectKey, operation, requestId, responseExpected);
    encode(encoder);
    encoder.setGIOPHeader(MessageType::REQUEST);
    stub->connection->send((void *)encoder.buffer.data(), encoder.buffer.offset);
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
        println("ORB::socketRcvd()");
        hexdump(buffer, size);
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
                cout << "REQUEST(requestId=" << request->requestId << ", objectKey='" << request->objectKey << "', " << request->operation << ")" << endl;
            }
            auto servant = servants.find(request->objectKey);  // FIXME: avoid string copy
            if (servant == servants.end()) {
                if (request->responseExpected) {
                    CORBA::GIOPEncoder encoder(connection);
                    encoder.skipReplyHeader();

                    encoder.writeString("IDL:omg.org/CORBA/OBJECT_NOT_EXIST:1.0");
                    encoder.writeUlong(0x4f4d0001);  // Attempt to pass an unactivated (unregistered) value as an object reference.
                    encoder.writeUlong(NO);          // completionStatus

                    auto length = encoder.buffer.offset;
                    encoder.setGIOPHeader(MessageType::REPLY);
                    encoder.setReplyHeader(request->requestId, ReplyStatus::SYSTEM_EXCEPTION);

                    connection->send((void *)encoder.buffer.data(), length);
                }
                return;
            }

            if (request->operation == "_is_a") {
                auto repositoryId = decoder.readString();
                CORBA::GIOPEncoder encoder(connection);
                encoder.skipReplyHeader();

                encoder.writeBoolean(repositoryId == servant->second->repository_id());

                auto length = encoder.buffer.offset;
                encoder.setGIOPHeader(MessageType::REPLY);
                encoder.setReplyHeader(request->requestId, ReplyStatus::NO_EXCEPTION);
                connection->send((void *)encoder.buffer.data(), length);

                return;
            }

            try {
                auto encoder = make_shared<CORBA::GIOPEncoder>(connection);
                encoder->skipReplyHeader();
                bool responseExpected = request->responseExpected;
                uint32_t requestId = request->requestId;

                // move parts of this into a separate function so that it can be unit tested
                // std::cerr << "CALL SERVANT" << std::endl;
                servant->second->_call(request->operation, decoder, *encoder)
                    .thenOrCatch(
                        [encoder, connection, responseExpected, requestId] {  // FIXME: the references objects won't be available
                            // println("SERVANT RETURNED");
                            if (responseExpected) {
                                // println("SERVANT WANTS RESPONSE");
                                auto length = encoder->buffer.offset;
                                encoder->setGIOPHeader(MessageType::REPLY);
                                encoder->setReplyHeader(requestId, ReplyStatus::NO_EXCEPTION);
                                // println("ORB::socketRcvd(): send REPLY via connection->send(...)");
                                connection->send((void *)encoder->buffer.data(), length);
                            }
                        },
                        [encoder, connection, responseExpected, requestId](std::exception_ptr eptr) {  // FIXME: the references objects won't be available
                            try {
                                // std::rethrow_exception(ex);
                                std::rethrow_exception(eptr);
                            } catch (CORBA::UserException &ex) {
                                println("SERVANT THREW CORBA::USER EXCEPTION");
                                if (responseExpected) {
                                    auto length = encoder->buffer.offset;
                                    encoder->setGIOPHeader(MessageType::REPLY);
                                    encoder->setReplyHeader(requestId, ReplyStatus::USER_EXCEPTION);
                                    connection->send((void *)encoder->buffer.data(), length);
                                }
                            } catch (CORBA::SystemException &error) {
                                println("SERVANT THREW CORBA::SYSTEM EXCEPTION");
                                if (responseExpected) {
                                    encoder->writeString(error.major());
                                    encoder->writeUlong(error.minor);
                                    encoder->writeUlong(error.completed);
                                    auto length = encoder->buffer.offset;
                                    encoder->setGIOPHeader(MessageType::REPLY);
                                    encoder->setReplyHeader(requestId, ReplyStatus::SYSTEM_EXCEPTION);
                                    connection->send((void *)encoder->buffer.data(), length);
                                }
                            } catch (std::exception &ex) {
                                println("SERVANT THREW STD::EXCEPTION");
                                if (responseExpected) {
                                    encoder->writeString("IDL:mark13.org/CORBA/GENERIC:1.0");
                                    encoder->writeUlong(0);
                                    encoder->writeUlong(0);
                                    encoder->writeString(format("IDL:{}:1.0: {}", typeid(ex).name(), ex.what()));
                                    auto length = encoder->buffer.offset;
                                    encoder->setGIOPHeader(MessageType::REPLY);
                                    encoder->setReplyHeader(requestId, ReplyStatus::SYSTEM_EXCEPTION);
                                    connection->send((void *)encoder->buffer.data(), length);
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
        default:
            cout << "ORB::socketRcvd(): GOT YET UNIMPLEMENTED REQUEST OF TYPE " << static_cast<unsigned>(type) << endl;
            break;
    }
}

}  // namespace CORBA
