#include "createAcceptKey.hh"

#include <nettle/base64.h>
#include <nettle/sha.h>

#include <fstream>

std::string sha1(const std::string& src) {
    sha1_ctx ctx;
    sha1_init(&ctx);
    sha1_update(&ctx, src.size(), reinterpret_cast<const uint8_t*>(src.c_str()));
    uint8_t temp[SHA1_DIGEST_SIZE];
    sha1_digest(&ctx, SHA1_DIGEST_SIZE, temp);
    std::string res(&temp[0], &temp[SHA1_DIGEST_SIZE]);
    return res;
}

std::string base64(const std::string& src) {
    base64_encode_ctx ctx;
    base64_encode_init(&ctx);
    int dstlen = BASE64_ENCODE_RAW_LENGTH(src.size());
    char* dst = new char[dstlen];
    base64_encode_raw(dst, src.size(), reinterpret_cast<const uint8_t*>(src.c_str()));
    std::string res(&dst[0], &dst[dstlen]);
    delete[] dst;
    return res;
}

std::string get_random16() {
    char buf[16];
    std::fstream f("/dev/urandom");
    f.read(buf, 16);
    return std::string(buf, buf + 16);
}

std::string create_acceptkey(const std::string& clientkey) {
    std::string s = clientkey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    return base64(sha1(s));
}

std::string create_clientkey() {
    return base64(get_random16());
}
