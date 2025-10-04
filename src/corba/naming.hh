#pragma once

#include <map>
#include <memory>
#include <string>

#include "skeleton.hh"
#include "stub.hh"

namespace CORBA {

class IOR;

class NamingContextExtImpl : public Skeleton {
        std::map<std::string, std::shared_ptr<Object>> name2Object;

    public:
        NamingContextExtImpl() : Skeleton() {}
        ~NamingContextExtImpl() {
            std::println("NamingContextExtImpl::~NamingContextExtImpl()");
        }
        virtual std::string_view repository_id() const override;

        void bind(const std::string &name, std::shared_ptr<Object> servant);
        std::shared_ptr<Object> resolve(const std::string &name);

    protected:
        void _orb_resolve(GIOPDecoder &decoder, GIOPEncoder &encoder);
        void _orb_resolve_str(GIOPDecoder &decoder, GIOPEncoder &encoder);
        CORBA::async<> _dispatch(const std::string_view &operation, GIOPDecoder &decoder, GIOPEncoder &encoder) override;
};

class NamingContextExtStub : public Stub {
    public:
        virtual std::string_view repository_id() const override;

        async<std::shared_ptr<IOR>> resolve_str(const std::string &name);
};

}  // namespace CORBA
