#include "../src/corba/blob.hh"
#include <map>

#include "kaffeeklatsch.hh"

using CORBA::blob, CORBA::blob_view;
using namespace std;
using namespace kaffeeklatsch;

kaffeeklatsch_spec([] {
    describe("blob & blob_view", [] {
        it("blob(const char *buffer, size_t nbytes) makes a copy of buffer", [] {
            auto raw = "hello";
            blob id(raw, 3);
            expect(id.data()).to.not_().equal((std::byte *)raw);
        });
        it("blob_view(const char *buffer, size_t nbytes) references buffer", [] {
            auto raw = "hello";
            blob_view id(raw, 3);
            expect(id.data()).to.equal(reinterpret_cast<const std::byte *>(raw));
        });
        it("we can create a blob from a blob_view", [] {
            blob_view id0("hello");
            blob id1(id0);
        });
        it("we ostream << blob", [] {
            blob id("\x0b\x0c\x00\x0d\x0e", 6);
            ostringstream s;
            s << id;
            expect(s.str()).to.equal("0b0c000d0e00");
        });
        it("we can format(...) a blob", [] {
            blob id("\x0b\x0c\x00\x0d\x0e", 6);
            auto s = std::format("{}", id);
            expect(s).to.equal("0b0c000d0e00");
        });
        it("we ostream << blob_view", [] {
            blob_view id("\x0b\x0c\x00\x0d\x0e", 6);
            ostringstream s;
            s << id;
            expect(s.str()).to.equal("0b0c000d0e00");
        });
        it("we can format(...) a blob_view", [] {
            blob_view id("\x0b\x0c\x00\x0d\x0e", 6);
            auto s = std::format("{}", id);
            expect(s).to.equal("0b0c000d0e00");
        });
        it("blob can be a map key", [] {
            std::map<blob, unsigned> m;
            m[blob("\x00\x01", 2)] = 1;
            m[blob("\x00\x02", 2)] = 2;
            m[blob("\x00\x03", 2)] = 3;
            expect(m[blob("\x00\x01", 2)]).to.equal(1);
            expect(m[blob("\x00\x02", 2)]).to.equal(2);
            expect(m[blob("\x00\x03", 2)]).to.equal(3);
        });
        it("blob_view can find blob as map key", [] {
            std::map<blob, unsigned> m;
            m[blob("\x00\x01", 2)] = 1;
            m[blob("\x00\x02", 2)] = 2;
            m[blob("\x00\x03", 2)] = 3;
            expect(m[blob_view("\x00\x01", 2)]).to.equal(1);
            expect(m[blob_view("\x00\x02", 2)]).to.equal(2);
            expect(m[blob_view("\x00\x03", 2)]).to.equal(3);
        });
        // we can use blob as a map key
        // we can use blob_view to find a blob used as a map_key
    });
});