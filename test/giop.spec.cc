#include "../src/corba/giop.hh"

#include "util.hh"

import std;

#include "kaffeeklatsch.hh"

using namespace kaffeeklatsch;
using namespace std;

kaffeeklatsch_spec([] {
    describe("GIOPEncoder", [] {
        describe("IIOP 1.2", [] {
                it("Request", [] {
                    CORBA::GIOPEncoder encoder;
                    encoder.majorVersion = 1;
                    encoder.minorVersion = 2;

                    encoder.encodeRequest(CORBA::blob("\x01\x02\x03\x04"), "myMethod", 4, true);
                    encoder.setGIOPHeader(CORBA::MessageType::REQUEST);
                    auto length = encoder.buffer.offset;

                    // hexdump(encoder.buffer.data(), length);

                    CORBA::CDRDecoder cdr(encoder.buffer.data(), length);
                    CORBA::GIOPDecoder decoder(cdr);
                    auto type = decoder.scanGIOPHeader();
                    expect(decoder.m_type).to.equal(type);
                    expect(decoder.m_type).to.equal(CORBA::MessageType::REQUEST);
                    expect(decoder.majorVersion).to.equal(1);
                    expect(decoder.minorVersion).to.equal(2);
                    expect(decoder.m_length + 12).to.equal(length);

                    auto request = decoder.scanRequestHeader();
                    expect(request->requestId).to.equal(4);
                    expect(request->objectKey).to.equal(CORBA::blob_view("\x01\x02\x03\x04"));
                    expect(request->operation).to.equal("myMethod");
                    expect(request->responseExpected).to.beTrue();
                });
        });
    });
    describe("GIOPDecoder", [] {
        it("OmniORB LocateRequest", []() {
            // OmniORB, IIOP 1.2, LocateRequest
            // 4749 4f50 0102 0103 2000 0000 0200 0000
            // ^         ^    ^  ^ ^         ^
            // |         |    |  | length 0x20 in little endian
            // |         |    |  message type
            // |         |    endian: 0 big, 1: little
            // |         version 1.2
            // "GIOP"

            auto data = parseOmniDump(R"(
                4749 4f50 0102 0103 2000 0000 0200 0000 GIOP.... .......
                0000 0000 1400 0000 ff62 6964 6972 fe97 .........bidir..
                c46b 6101 000f 5700 0000 0000           .ka...W.....)");
            CORBA::CDRDecoder dataview((char *)data.data(), data.size());
            CORBA::GIOPDecoder decoder(dataview);

            auto type = decoder.scanGIOPHeader();
            expect(type).equals(CORBA::MessageType::LOCATE_REQUEST);
            expect(decoder.m_type).equals(CORBA::MessageType::LOCATE_REQUEST);
            expect(decoder.majorVersion).equals(1);
            expect(decoder.minorVersion).equals(2);
            expect(decoder.m_length + 12).equals(data.size());
            expect(dataview.getOffset()).equals(12);

            auto locateRequest = decoder.scanLocateRequest();
            expect(locateRequest->requestId).equals(2);
            CORBA::blob_view objectKey(dataview.data() + 24, 20);
            expect(locateRequest->objectKey).equals(objectKey);
        });
        it("OmniORB Request", [] {
            auto data = parseOmniDump(R"(
                4749 4f50 0102 0100 e000 0000 0400 0000 GIOP............
                0300 0000 0000 0000 1400 0000 ff62 6964 .............bid
                6972 fe97 c46b 6101 000f 5700 0000 0000 ir...ka...W.....
                0b00 0000 7365 6e64 4f62 6a65 6374 0000 ....sendObject..
                0100 0000 0100 0000 0c00 0000 0100 0000 ................
                0100 0100 0901 0100 1200 0000 4944 4c3a ............IDL:
                4749 4f50 536d 616c 6c3a 312e 3000 0000 GIOPSmall:1.0...
                0100 0000 0000 0000 6800 0000 0101 0200 ........h.......
                0e00 0000 3139 322e 3136 382e 312e 3130 ....192.168.1.10
                3500 bbcf 1400 0000 ff62 6964 6972 fea8 5........bidir..
                c46b 6101 000f 7500 0000 0000 0200 0000 .ka...u.........
                0000 0000 0800 0000 0100 0000 0054 5441 .............TTA
                0100 0000 1c00 0000 0100 0000 0100 0100 ................
                0100 0000 0100 0105 0901 0100 0100 0000 ................
                0901 0100 0400 0000 666f 6f00           ........foo.)");
            CORBA::CDRDecoder dataview((char *)data.data(), data.size());
            CORBA::GIOPDecoder decoder(dataview);

            auto type = decoder.scanGIOPHeader();
            expect(type).equals(CORBA::MessageType::REQUEST);
            expect(decoder.m_type).equals(CORBA::MessageType::REQUEST);
            expect(decoder.majorVersion).equals(1);
            expect(decoder.minorVersion).equals(2);
            expect(decoder.m_length + 12).equals(data.size());
            expect(dataview.getOffset()).equals(12);

            auto request = decoder.scanRequestHeader();
            expect(request->responseExpected).equals(true);
            expect(request->requestId).equals(4);
            CORBA::blob_view objectKey(dataview.data() + 28, 20);
            expect(request->objectKey).equals(objectKey);
            // hexdump(request->operation);
            expect(request->operation).equals("sendObject");
        });
        it("OmniORB Reply", [] {
            auto data = parseOmniDump(R"(
                4749 4f50 0102 0101 0c00 0000 0400 0000 GIOP............
                0000 0000 0000 0000                     ........)");
            CORBA::CDRDecoder dataview((char *)data.data(), data.size());
            CORBA::GIOPDecoder decoder(dataview);

            auto type = decoder.scanGIOPHeader();
            expect(type).equals(CORBA::MessageType::REPLY);
            expect(decoder.m_type).equals(CORBA::MessageType::REPLY);
            expect(decoder.majorVersion).equals(1);
            expect(decoder.minorVersion).equals(2);
            expect(decoder.m_length + 12).equals(data.size());
            expect(dataview.getOffset()).equals(12);

            auto reply = decoder.scanReplyHeader();
            expect(reply->requestId).to.equal(4);
            expect(reply->replyStatus).to.equal(CORBA::ReplyStatus::NO_EXCEPTION);
        });
    });
});
