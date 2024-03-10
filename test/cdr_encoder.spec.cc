#include "../src/corba/cdr.hh"
#include "kaffeeklatsch.hh"

using namespace kaffeeklatsch;

kaffeeklatsch_spec([] {
    describe("CDREncoder", [] {
        it("boolean", [] {
            CORBA::CDREncoder encoder;
            encoder.writeBoolean(0x42);
            CORBA::CDRDecoder decoder(encoder);
            expect(decoder.readOctet()).equals(0x01);
        });
        it("octet", [] {
            CORBA::CDREncoder encoder;
            encoder.writeOctet(0x42);
            CORBA::CDRDecoder decoder(encoder);
            expect(decoder.readOctet()).equals(0x42);
        });
        it("ushort", [] {
            CORBA::CDREncoder encoder;
            encoder.writeUshort(0xDEAD);
            CORBA::CDRDecoder decoder(encoder);
            expect(decoder.readUshort()).equals(0xDEAD);
        });
        it("ulong", [] {
            CORBA::CDREncoder encoder;
            encoder.writeUlong(0xDEADBEEF);
            CORBA::CDRDecoder decoder(encoder);
            expect(decoder.readUlong()).equals(0xDEADBEEF);
        });
        it("ulonglong", [] {
            CORBA::CDREncoder encoder;
            encoder.writeUlonglong(0xDEADBEEFC0DEBABE);
            CORBA::CDRDecoder decoder(encoder);
            expect(decoder.readUlonglong()).equals(0xDEADBEEFC0DEBABE);
        });
    });
});
