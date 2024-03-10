#pragma once

#include <string>
#include <variant>
#include <vector>

namespace CORBA {

// the actual IOR.idl is more complex, one OID plus a list of components
// struct IOR {
//         std::string oid;

//         std::string host;
//         uint16_t port;
//         std::string objectKey;

//         IOR(const std::string &ior);
// };

struct ObjectAddress {
        std::string proto = "iiop";
        uint8_t major = 1;
        uint8_t minor = 0;
        std::string host;
        uint16_t port = 2809;
};

struct CorbaLocation {
        std::vector<ObjectAddress> addr;
        std::string objectKey;
        virtual std::string str() const;

    protected:
        std::string _str() const;
};

struct CorbaName : CorbaLocation {
        CorbaName() { objectKey = "NameService"; }
        std::string name;
        virtual std::string str() const;
};

class IOR;

std::variant<IOR, CorbaLocation, CorbaName> decodeURI(const std::string &uri);

}  // namespace CORBA
