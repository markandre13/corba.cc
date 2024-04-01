#include "interface_skel.hh"

class Interface_impl : public Interface_skel {
    public:
        Interface_impl(std::shared_ptr<CORBA::ORB> orb) : Interface_skel(orb) {}

        CORBA::async<bool> callBoolean(bool value) override { co_return value; }
        CORBA::async<uint8_t> callOctet(uint8_t value) override { co_return value; }  // check uint8_t with real CORBA

        CORBA::async<uint16_t> callUShort(uint16_t value) override { co_return value; }
        CORBA::async<uint32_t> callUnsignedLong(uint32_t value) override { co_return value; }
        CORBA::async<uint64_t> callUnsignedLongLong(uint64_t value) override { co_return value; }

        CORBA::async<int16_t> callShort(int16_t value) override { co_return value; }
        CORBA::async<int32_t> callLong(int32_t value) override { co_return value; }
        CORBA::async<int64_t> callLongLong(int64_t value) override { co_return value; }

        CORBA::async<float> callFloat(float value) override { co_return value; }
        CORBA::async<double> callDouble(double value) override { co_return value; }

        CORBA::async<std::string> callString(const std::string_view &value) override { co_return std::string(value); }
        CORBA::async<CORBA::blob> callBlob(const CORBA::blob_view &value) override { co_return CORBA::blob(value); }

        // receiving sequence<float> can directly map to the received packet with std::span<float>.
        // returning sequence<float> is not ideal as the idl does not known if it's is temporary, hence it can not be std::span.
        // i should have a look at the original c++ corba mapping, protobuf and cap'n proto.
        // it might not be that terrible since oop should follow the 'tell, don't ask rule'
        CORBA::async<std::vector<float>> callSeqFloat(const std::span<float> & value) override { co_return std::vector(value.begin(), value.end()); }
        CORBA::async<std::vector<double>> callSeqDouble(const std::span<double> & value) override { co_return std::vector(value.begin(), value.end()); }
        
        // receiving sequence<string> adds more overhead since the memory layout differs from the c++ one
        // returning sequence<string> needs to create a full copy, which corba then has to copy again.
        CORBA::async<std::vector<std::string>> callSeqString(const std::vector<std::string_view> & in) override { 
            std::vector<std::string> out;
            out.reserve(in.size());
            for(auto &p: in) {
                out.emplace_back(p);
            }
            co_return out;
        };

        std::shared_ptr<Peer> peer;
        CORBA::async<void> setPeer(std::shared_ptr<Peer> aPeer) override {
            this->peer = aPeer;
            co_return;
        }
        CORBA::async<std::string> callPeer(const std::string_view &value) override {
            auto s = co_await peer->callString(std::string(value) + " to the");
            co_return s + ".";
        }
        // next steps:
        // [X] set/get callback object and call it
        // [ ] use ArrayBuffer/Buffer for sequence<octet> for the javascript side
        // [ ] completeness: signed & floating point
};

class Peer_impl : public Peer_skel {
    public:
        Peer_impl(std::shared_ptr<CORBA::ORB> orb) : Peer_skel(orb) {}
        CORBA::async<std::string> callString(const std::string_view &value) override { co_return std::string(value) + " world"; }
};
