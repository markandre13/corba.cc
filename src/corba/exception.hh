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

class MARSHAL : public SystemException {
    public:
        MARSHAL(uint32_t minor, CompletionStatus completed) : SystemException(minor, completed) {}
        const char *_rep_id() const noexcept override { return "IDL:omg.org/CORBA/MARSHAL:1.0"; }
};

class NO_PERMISSION : public SystemException {
    public:
        NO_PERMISSION(uint32_t minor, CompletionStatus completed) : SystemException(minor, completed) {}
        const char *_rep_id() const noexcept override { return "IDL:omg.org/CORBA/NO_PERMISSION:1.0"; }
};

class BAD_PARAM : public SystemException {
    public:
        BAD_PARAM(uint32_t minor, CompletionStatus completed) : SystemException(minor, completed) {}
        const char *_rep_id() const noexcept override { return "IDL:omg.org/CORBA/BAD_PARAM:1.0"; }
};

class BAD_OPERATION : public SystemException {
    public:
        BAD_OPERATION(uint32_t minor, CompletionStatus completed) : SystemException(minor, completed) {}
        const char *_rep_id() const noexcept override { return "IDL:omg.org/CORBA/BAD_OPERATION:1.0"; }
};

class OBJECT_NOT_EXIST : public SystemException {
    public:
        OBJECT_NOT_EXIST(uint32_t minor, CompletionStatus completed) : SystemException(minor, completed) {}
        const char *_rep_id() const noexcept override { return "IDL:omg.org/CORBA/OBJECT_NOT_EXIST:1.0"; }
};

class TRANSIENT : public SystemException {
    public:
        TRANSIENT(uint32_t minor, CompletionStatus completed) : SystemException(minor, completed) {}
        const char *_rep_id() const noexcept override { return "IDL:omg.org/CORBA/TRANSIENT:1.0"; }
};

class OBJECT_ADAPTER : public SystemException {
    public:
        OBJECT_ADAPTER(uint32_t minor, CompletionStatus completed) : SystemException(minor, completed) {}
        const char *_rep_id() const noexcept override { return "IDL:omg.org/CORBA/OBJECT_ADAPTER:1.0"; }
};

// raised when effective RebindPolicy has value NO_REBIND or NO_RECONNECT
class REBIND : public SystemException {
    public:
        REBIND(uint32_t minor, CompletionStatus completed) : SystemException(minor, completed) {}
        const char *_rep_id() const noexcept override { return "IDL:omg.org/CORBA/REBIND:1.0"; }
};

class NO_IMPLEMENT : public SystemException {
    public:
        NO_IMPLEMENT(uint32_t minor, CompletionStatus completed) : SystemException(minor, completed) {}
        const char *_rep_id() const noexcept override { return "IDL:omg.org/CORBA/NO_IMPLEMENT:1.0"; }
};

class COMM_FAILURE : public SystemException {
    public:
        COMM_FAILURE(uint32_t minor, CompletionStatus completed) : SystemException(minor, completed) {}
        const char *_rep_id() const noexcept override { return "IDL:omg.org/CORBA/COMM_FAILURE:1.0"; }
};

class INITIALIZE : public SystemException {
    public:
        INITIALIZE(uint32_t minor, CompletionStatus completed) : SystemException(minor, completed) {}
        const char *_rep_id() const noexcept override { return "IDL:omg.org/CORBA/INITIALIZE:1.0"; }
};

#define INITIALIZE_TransportError 0x4154000d

}  // namespace CORBA