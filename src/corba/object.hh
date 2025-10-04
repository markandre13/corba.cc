#pragma once

#include "blob.hh"

#include <memory>
#include <string>

namespace CORBA {

class ORB;

// CORBA 3.3 Part 1 Interfaces, 8.3 Object Reference Operations
class Object {
    public:
        virtual ~Object();
        virtual std::string_view repository_id() const;
        virtual blob_view get_object_key() const = 0;
        virtual std::shared_ptr<CORBA::ORB> get_ORB() const = 0;
        virtual bool _is_a(const std::string_view &repository_id) const;
};

}  // namespace CORBA
