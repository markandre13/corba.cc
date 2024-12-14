#pragma once

#include <cstdint>
#include <stdexcept>

namespace CORBA {

enum CompletionStatus { YES, NO, MAYBE };

class Exception : public std::exception {};

/*
 * base class for exceptions defined by the user in an IDL file
 */
class UserException : public Exception {};

/*
 * Base class for all CORBA related exceptions.
 *
 * See CORBA 3.3; Part 1: Interface; A.6 Exception Codes
 */

class SystemException : public Exception {
    public:
        const uint32_t minor;
        const CompletionStatus completed;
        SystemException(uint32_t minor, CompletionStatus completed) : minor(minor), completed(completed) {}
        virtual const char *_rep_id() const noexcept = 0;
        virtual const char* what() const noexcept override { return _rep_id(); }
};

// #define OMGMinorCode(x) (0x4f4d0000 | x)
// #define OMNIORBMinorCode(x) (0x41540000 | x)

// ACTIVITY_COMPLETED
// ACTIVITY_REQUIRED
// BAD_CONTEXT

class BAD_INV_ORDER : public SystemException {
    public:
        BAD_INV_ORDER(uint32_t minor, CompletionStatus completed) : SystemException(minor, completed) {}
        const char *_rep_id() const noexcept override { return "IDL:omg.org/CORBA/BAD_INV_ORDER:1.0"; }
};

class BAD_OPERATION : public SystemException {
    public:
        BAD_OPERATION(uint32_t minor, CompletionStatus completed) : SystemException(minor, completed) {}
        const char *_rep_id() const noexcept override { return "IDL:omg.org/CORBA/BAD_OPERATION:1.0"; }
};

/**
 * An invalid parameter was passed.
 */
class BAD_PARAM : public SystemException {
    public:
        BAD_PARAM(uint32_t minor, CompletionStatus completed) : SystemException(minor, completed) {}
        const char *_rep_id() const noexcept override { return "IDL:omg.org/CORBA/BAD_PARAM:1.0"; }
};

class BAD_TYPECODE : public SystemException {
    public:
        BAD_TYPECODE(uint32_t minor, CompletionStatus completed) : SystemException(minor, completed) {}
        const char *_rep_id() const noexcept override { return "IDL:omg.org/CORBA/BAD_TYPECODE:1.0"; }
};

// CODESET_INCOMPATIBLE
// DATA_CONVERSION
// IMP_LIMIT

/**
 * ORB initialization failure
 */
class INITIALIZE : public SystemException {
    public:
        INITIALIZE(uint32_t minor, CompletionStatus completed) : SystemException(minor, completed) {}
        const char *_rep_id() const noexcept override { return "IDL:omg.org/CORBA/INITIALIZE:1.0"; }
};

#define INITIALIZE_TransportError 0x4154000d

// INTERNAL
// INTF_REPOS
// INVALID_ACTIVITY
// INV_OBJREF
// INV_POLICY

/**
 * Error marshaling parameter and/or result.
 */
class MARSHAL : public SystemException {
    public:
        MARSHAL(uint32_t minor, CompletionStatus completed) : SystemException(minor, completed) {}
        const char *_rep_id() const noexcept override { return "IDL:omg.org/CORBA/MARSHAL:1.0"; }
};

class NO_IMPLEMENT : public SystemException {
    public:
        NO_IMPLEMENT(uint32_t minor, CompletionStatus completed) : SystemException(minor, completed) {}
        const char *_rep_id() const noexcept override { return "IDL:omg.org/CORBA/NO_IMPLEMENT:1.0"; }
};

// NO_RESOURCES
// NO_RESPONSE

class OBJECT_ADAPTER : public SystemException {
    public:
        OBJECT_ADAPTER(uint32_t minor, CompletionStatus completed) : SystemException(minor, completed) {}
        const char *_rep_id() const noexcept override { return "IDL:omg.org/CORBA/OBJECT_ADAPTER:1.0"; }
};

/**
 * Non-existent object: delete reference.
 */
class OBJECT_NOT_EXIST : public SystemException {
    public:
        OBJECT_NOT_EXIST(uint32_t minor, CompletionStatus completed) : SystemException(minor, completed) {}
        const char *_rep_id() const noexcept override { return "IDL:omg.org/CORBA/OBJECT_NOT_EXIST:1.0"; }
};
// TIMEOUT
// TRANSACTION_ROLLEDBACK

/**
 * Transient failure: Reissue request.
 * 
 * Indicates that the ORB attempted to reach an object and failed. It is not an indication that an object does not exist.
 * Instead, it simply means that no further determination of an object's status was possible because it could not be reached.
 * 
 * This exception is raised if an attempt to establish a connection fails, for example, because the server or the implementation
 * repository is down.
 */
class TRANSIENT : public SystemException {
    public:
        TRANSIENT(uint32_t minor, CompletionStatus completed) : SystemException(minor, completed) {}
        const char *_rep_id() const noexcept override { return "IDL:omg.org/CORBA/TRANSIENT:1.0"; }
};

// UNKNOWN

// THE FOLLOWING ARE NOT IN CORBA 3.3

/**
 * No permission for attempted operation.
 */
class NO_PERMISSION : public SystemException {
    public:
        NO_PERMISSION(uint32_t minor, CompletionStatus completed) : SystemException(minor, completed) {}
        const char *_rep_id() const noexcept override { return "IDL:omg.org/CORBA/NO_PERMISSION:1.0"; }
};

/**
 * Rebind needed.
 * 
 * Raised when effective RebindPolicy has value NO_REBIND or NO_RECONNECT.
 * 
 * (removed in CORBA 3.3)
 */
class REBIND : public SystemException {
    public:
        REBIND(uint32_t minor, CompletionStatus completed) : SystemException(minor, completed) {}
        const char *_rep_id() const noexcept override { return "IDL:omg.org/CORBA/REBIND:1.0"; }
};

/**
 * Communication failure.
 * 
 * This exception is raised if communication is lost while an operation is in progress, after the request
 * was sent by the client, but before the reply from the server has been returned to the client.
 * 
 * (removed in CORBA 3.3, i guess TIMEOUT can take it's place)
 */
class COMM_FAILURE : public SystemException {
    public:
        COMM_FAILURE(uint32_t minor, CompletionStatus completed) : SystemException(minor, completed) {}
        const char *_rep_id() const noexcept override { return "IDL:omg.org/CORBA/COMM_FAILURE:1.0"; }
};

}  // namespace CORBA
