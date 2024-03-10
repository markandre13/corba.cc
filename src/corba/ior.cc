#include "ior.hh"
#include "giop.hh"

#include <charconv>

using namespace std;

namespace CORBA {

IOR::IOR(const string &ior) {
    // 7.6.9 Stringified Object References

    // Standard stringified IOR format
    if (ior.compare(0, 4, "IOR:") != 0) {
        throw runtime_error(format(R"(Missing "IOR:" prefix in "{}".)", ior));
    }
    if (ior.size() & 1) {
        throw runtime_error("IOR has a wrong length.");
    }
    // decode hex string
    auto bufferSize = (ior.size() - 4) / 2;
    char buffer[bufferSize];
    auto ptr = ior.c_str();
    for (size_t i = 4, j = 0; i < ior.size(); i += 2, ++j) {
        unsigned char out;
        std::from_chars(ptr + i, ptr + i + 2, out, 16);
        buffer[j] = out;
    }
    // decode reference
    CORBA::CDRDecoder cdr(buffer, bufferSize);
    CORBA::GIOPDecoder giop(cdr);
    giop.readEndian();
    auto ref = giop.readReference();
    oid = ref->oid;
    host = ref->host;
    port = ref->port;
    objectKey = ref->objectKey;
}

IOR::~IOR() {}

}  // namespace CORBA
