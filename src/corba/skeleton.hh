#pragma once

#include "object.hh"
#include "coroutine.hh"

namespace CORBA {

class GIOPDecoder;
class GIOPEncoder;

/**
 * Base class for representing object implementations.
 */
class Skeleton : public virtual Object, public std::enable_shared_from_this<Skeleton> {
    public:
        friend class ORB;
        std::shared_ptr<CORBA::ORB> orb;
        blob objectKey;

    // public:
        Skeleton() {}
        Skeleton(std::shared_ptr<CORBA::ORB> orb) {}                                // the ORB will create an objectKey
        Skeleton(std::shared_ptr<CORBA::ORB> orb, const std::string &objectKey) {}  // for special objectKeys, e.g. "omg.org/CosNaming/NamingContextExt"
        virtual ~Skeleton() override;
        virtual blob_view get_object_key() const override { return objectKey; }
        std::shared_ptr<CORBA::ORB> get_ORB() const override { return orb; }
        virtual CORBA::async<> _dispatch(const std::string_view &operation, GIOPDecoder &decoder, GIOPEncoder &encoder) = 0;
};

}  // namespace CORBA
