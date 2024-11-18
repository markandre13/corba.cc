#include <print>

#include "interface_skel.hh"

class Interface_impl : public Interface_skel {
        std::string _rwattr;

    public:
        Interface_impl(std::shared_ptr<CORBA::ORB> orb) : Interface_skel(orb), _rwattr("hello") {}

        virtual CORBA::async<std::string> roAttribute() override { co_return std::string("static"); }
        virtual CORBA::async<std::string> rwAttribute() override { co_return _rwattr; }
        virtual CORBA::async<void> rwAttribute(const std::string_view &value) override {
            _rwattr = value;
            co_return;
        }

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

        CORBA::async<RGBA> callStruct(const RGBA &value) override { co_return value; }

        CORBA::async<std::vector<float>> callSeqFloat(const std::span<float> &value) override { co_return std::vector(value.begin(), value.end()); }
        CORBA::async<std::vector<double>> callSeqDouble(const std::span<double> &value) override { co_return std::vector(value.begin(), value.end()); }
        CORBA::async<std::vector<std::string>> callSeqString(const std::vector<std::string_view> &in) override {
            std::vector<std::string> out;
            out.reserve(in.size());
            for (auto &p : in) {
                out.emplace_back(p);
            }
            co_return out;
        };
        CORBA::async<std::vector<RGBA>> callSeqRGBA(const std::vector<RGBA> &value) override { co_return value; }

        std::shared_ptr<Peer> peer;
        CORBA::async<void> setPeer(std::shared_ptr<Peer> aPeer) override {
            this->peer = aPeer;
            co_return;
        }
        CORBA::async<std::string> callPeer(const std::string_view &value) override {
            auto s = co_await peer->callString(std::string(value) + " to the");
            co_return s + ".";
        }
};

class Peer_impl : public Peer_skel {
    public:
        Peer_impl(std::shared_ptr<CORBA::ORB> orb) : Peer_skel(orb) {}
        CORBA::async<std::string> callString(const std::string_view &value) override { co_return std::string(value) + " world"; }
};

class Client_impl : public Client_skel {
    public:
        Client_impl(std::shared_ptr<CORBA::ORB> orb) : Client_skel(orb) {}
        CORBA::async<> ping() override {
            std::println("got ping");
            co_return;
        }
};
