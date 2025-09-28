#pragma once

#include "object.hh"

namespace CORBA {

namespace detail {
class Connection;
}

/**
 * Base class for representing remote objects.
 */
class Stub : public virtual Object {
        friend class ORB;
        std::shared_ptr<CORBA::ORB> orb;
        /**
         * objectKey used on the remote end of the connection
         */
        blob objectKey;
    public:
        /**
         * connection to where the remote object lives
         */
        std::shared_ptr<detail::Connection> connection; 

    public:
        void initStub(std::shared_ptr<CORBA::ORB> anOrb, const CORBA::blob_view &anObjectKey, std::shared_ptr<detail::Connection> aConnection) {
            this->orb = anOrb;
            this->objectKey = anObjectKey;
            this->connection = aConnection;
        }
        virtual ~Stub() override;
        virtual blob_view get_object_key() const override { return objectKey; }
        std::shared_ptr<CORBA::ORB> get_ORB() const override { return orb; }
};

}  // namespace CORBA
