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
        blob objectKey;                  // objectKey used on the remote end of the connection
        detail::Connection *connection;  // connection to where the remote object lives

    public:
        Stub(std::shared_ptr<CORBA::ORB> orb, const CORBA::blob_view &objectKey, detail::Connection *connection)
            : orb(orb), objectKey(objectKey), connection(connection) {}
        virtual ~Stub() override;
        virtual blob_view get_object_key() const override { return objectKey; }
        std::shared_ptr<CORBA::ORB> get_ORB() const override { return orb; }
};

}  // namespace CORBA
