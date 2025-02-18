#include <print>

#include "interface_skel.hh"

class RemoteObject_impl : public RemoteObject_skel {
        std::string _id;
        std::string _name;

    public:
        RemoteObject_impl(const std::string id, const std::string name) : _id(id), _name(name) {}
        CORBA::async<std::string> id() override { co_return _id; }
        CORBA::async<std::string> name() override { co_return _name; }
        CORBA::async<void> name(const std::string_view &name) override {
            _name = name;
            co_return;
        }
};

class Interface_impl : public Interface_skel {
        std::string _rwattr;
        std::vector<std::shared_ptr<RemoteObject>> remoteObjects;

    public:
        Interface_impl(std::shared_ptr<CORBA::ORB> orb) : _rwattr("hello") {
            auto o0 = std::make_shared<RemoteObject_impl>("1", "alpha");
            orb->activate_object(o0);
            remoteObjects.push_back(o0);
            auto o1 = std::make_shared<RemoteObject_impl>("2", "bravo");
            orb->activate_object(o1);
            remoteObjects.push_back(o1);
            auto o2 = std::make_shared<RemoteObject_impl>("3", "charly");
            orb->activate_object(o2);
            remoteObjects.push_back(o2);
        }

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
        void recvString(const std::string_view &value) override { std::println("Interface::recvString({} characters of '{}')", value.size(), value[0]); }
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
        // CORBA::async<std::vector<std::shared_ptr<Peer>>> getSeqPeer() override {
        //     std::vector<std::shared_ptr<Peer>> result;
        //     if (peer) {
        //         result.push_back(peer);
        //     }
        //     co_return result;
        // }
        CORBA::async<std::vector<std::shared_ptr<RemoteObject>>> getRemoteObjects() override { 
            co_return remoteObjects;
        }

        std::shared_ptr<Peer> peer;
        CORBA::async<void> setPeer(std::shared_ptr<Peer> aPeer) override {
            this->peer = aPeer;
            co_return;
        }
        CORBA::async<std::shared_ptr<Peer>> getPeer() override {
            co_return this->peer;
        }

        CORBA::async<std::string> callPeer(const std::string_view &value) override {
            auto s = co_await peer->callString(std::string(value) + " to the");
            co_return s + ".";
        }
};

class Peer_impl : public Peer_skel {
    public:
        CORBA::async<std::string> callString(const std::string_view &value) override { co_return std::string(value) + " world"; }
};

class Client_impl : public Client_skel {
    public:
        CORBA::async<> ping() override {
            std::println("got ping");
            co_return;
        }
};
