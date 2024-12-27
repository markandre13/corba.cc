#include "stub.hh"
#include "orb.hh"
#include "net/protocol.hh"

#include <print>

using namespace std;

namespace CORBA {

// IT IS TIME TO START WRITING A FREAKING BUNCH OF UNIT TESTS!!!

Stub::~Stub() {
    println("Stub::~Stub()");
    // orb->dump();

    // remove stub from it's connection
    if (connection) {
        println("Stub::~Stub(): we have a connection with {} stubs",  connection->stubsById.size());
        for (auto ptr = connection->stubsById.begin(); ptr != connection->stubsById.end(); ++ptr) {
            printf("Stub::~Stub(): try to remove stub '%s' from connection: this=%p stubById=%p\n", ptr->first.c_str(), this, ptr->second);
            if (ptr->second == this) {
                println("Object::~Object(): removing object from connection.stubsById");
                connection->stubsById.erase(ptr);
                break;
            } else {
                println("it's not this stub???");
            }
        }
        println("Stub::~Stub(): we NOW have a connection with {} stubs",  connection->stubsById.size());
    }
}

};  // namespace CORBA
