#include "naming.hh"
#include "orb.hh"
#include "hexdump.hh"
#include "giop.hh"
#include "ior.hh"

#include <stdexcept>
#include <iostream>

using namespace std;

namespace CORBA {

void NamingContextExtImpl::bind(const std::string &name, std::shared_ptr<Object> servant) {
    if (name2Object.contains(name)) {
        throw std::runtime_error(format("name \"{}\" is already bound to object", name));
    }
    name2Object[name] = servant;
}

std::shared_ptr<Object> NamingContextExtImpl::resolve(const std::string &name) {
    // println("NamingContextImpl.resolve(\"{}\")", name);
    auto servant = name2Object.find(name);
    if (servant == name2Object.end()) {
        println("NamingContextExtImpl::resolve(\"{}\"): name is not bound to an object", name);
        hexdump(name);
        throw std::runtime_error(format("NamingContextExtImpl::resolve(\"{}\"): name is not bound to an object", name));
    }
    return servant->second;
}

void NamingContextExtImpl::_orb_resolve(GIOPDecoder &decoder, GIOPEncoder &encoder) {
    auto entries = decoder.readUlong();
    auto name = decoder.readString();
    auto key = decoder.readString();
    if (entries != 1 && !key.empty()) {
        cerr << "warning: resolve got " << entries << " (expected 1) and/or key is \"" << key << "\" (expected \"\")" << endl;
    }
    std::shared_ptr<Object> result = resolve(name);  // FIXME: we don't want to copy the string
    encoder.writeObject(result.get());
}

void NamingContextExtImpl::_orb_resolve_str(GIOPDecoder &decoder, GIOPEncoder &encoder) {
    auto name = decoder.readString();
    auto result = resolve(name);
    encoder.writeObject(result.get());
}

async<> NamingContextExtImpl::_call(const std::string_view &operation, GIOPDecoder &decoder, GIOPEncoder &encoder) {
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
    throw std::runtime_error(std::format("bad operation: '{}' does not exist", operation));
}

async<std::shared_ptr<IOR>> NamingContextExtStub::resolve_str(const std::string &name) {
    return get_ORB()->twowayCall<shared_ptr<IOR>>(
        this, "resolve_str",
        [name](GIOPEncoder &encoder) {
            encoder.writeString(name);
        },
        [](GIOPDecoder &decoder) {
            return decoder.readReference();
        });
}

// FIXME: OmniORB want's "IDL:omg.org/CosNaming/NamingContext:1.0", but also "IDL:omg.org/CosNaming/NamingContextExt:1.0" possible
static std::string_view _rid("IDL:omg.org/CosNaming/NamingContext:1.0");
std::string_view NamingContextExtImpl::repository_id() const { return _rid; }
std::string_view NamingContextExtStub::repository_id() const { return _rid; }

}  // namespace CORBA
