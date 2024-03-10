#include "url.hh"
#include "cdr.hh"
#include "giop.hh"
#include "corba.hh"

#include <format>
#include <charconv>

using namespace std;

namespace CORBA {

string CorbaLocation::str() const { return "corbaloc:" + _str(); }

string CorbaName::str() const {
    auto txt = "corbaname:" + _str();
    if (name.size() > 0) {
        txt += "#" + name;
    }
    return txt;
}

string CorbaLocation::_str() const {
    string txt = "";
    // for (size_t i = 0; i < addr.size(); ++i) {
    for (auto &a : addr) {
        if (txt.size() != 0) {
            txt += ",";
        }
        if (a.proto == "iiop") {
            txt += format("{}:{}.{}@{}:{}", a.proto, a.major, a.minor, a.host, a.port);
        } else if (a.proto == "rir") {
            txt += "rir:";
        }
    }
    if (objectKey.size() != 0) {
        txt += "/" + objectKey;
    }
    return txt;
}

struct UrlLexer {
        std::string data;

    public:
        size_t pos = 0;

        UrlLexer(const string &url) : data(url) {}
        std::optional<unsigned> number();
        std::optional<string> uri();
        bool match(const char *);

        inline bool eof() { return pos >= data.size(); }
        inline char getc() { return data[pos++]; }
        inline void ungetc() { --pos; }
};

class UrlParser {
        UrlLexer url;

    public:
        UrlParser(const string &url) : url(url) {}
        std::variant<IOR, CorbaLocation, CorbaName> parse();

    protected:
        void corbaloc(CorbaLocation &);
        void corbaname(CorbaName &);
        void obj_addr_list(vector<ObjectAddress> &list);
        bool obj_addr(ObjectAddress &addr);
        bool prot_addr(ObjectAddress &addr);
        bool rir_prot_addr(ObjectAddress &addr);
        bool iiop_prot_addr(ObjectAddress &addr);
        bool iiop_id();
        bool iiop_addr(ObjectAddress &addr);
        bool version(ObjectAddress &addr);
        bool host(ObjectAddress &addr);
        bool label(ObjectAddress &addr);
        bool port(ObjectAddress &addr);
        string key_string();
};

std::variant<IOR, CorbaLocation, CorbaName> decodeURI(const std::string &uri) {
    return UrlParser(uri).parse();
}


std::optional<unsigned> UrlLexer::number() {
    auto start = pos;
    while (isdigit(getc())) {
    }
    ungetc();
    if (start == pos) {
        return {};
    }
    auto ptr = data.c_str();
    unsigned out;
    std::from_chars(ptr + start, ptr + pos, out, 10);
    return {out};
}

string _decodeURI(const string &SRC) {
    string ret;
    char ch;
    size_t i;
    int ii;
    for (i = 0; i < SRC.length(); i++) {
        if (SRC[i] == '%') {
            sscanf(SRC.substr(i + 1, 2).c_str(), "%x", &ii);
            ch = static_cast<char>(ii);
            ret += ch;
            i = i + 2;
        } else {
            ret += SRC[i];
        }
    }
    return ret;
}

std::optional<string> UrlLexer::uri() {
    auto start = pos;
    while (!eof()) {
        auto c = getc();
        if (!isalnum(c) && strchr("%;/:?:@&=+$,-_!~*’|()", c) == nullptr) {
            ungetc();
            break;
        }
    }
    if (pos == start) {
        return {};
    }
    return _decodeURI(data.substr(start, pos));
}

bool UrlLexer::match(const char *s) {
    auto length = strlen(s);
    if (pos + length > data.size()) {
        return false;
    }
    if (data.compare(pos, length, s) != 0) {
        return false;
    }
    pos += length;
    return true;
}

std::variant<IOR, CorbaLocation, CorbaName> UrlParser::parse() {
    if (url.match("IOR:")) {
        return IOR(url.data);
    }
    if (url.match("corbaloc:")) {
        CorbaLocation loc;
        corbaloc(loc);
        return loc;
    }
    if (url.match("corbaname:")) {
        CorbaName name;
        corbaname(name);
        return name;
    }
    throw runtime_error("Bad string, expected on of 'IOR:...', 'corbaloc:...' or 'corbaname:...'");
}

void UrlParser::corbaloc(CorbaLocation &loc) {
    obj_addr_list(loc.addr);
    if (url.match("/")) {
        loc.objectKey = key_string();
        // TODO: handle RFC 2396 escapes!!!
    }
}

void UrlParser::corbaname(CorbaName &name) {
    corbaloc(name);
    if (url.match("#")) {
        auto uri = url.uri();
        if (uri.has_value()) {
            name.name = uri.value();
        }
    }
}

void UrlParser::obj_addr_list(vector<ObjectAddress> &list) {
    do {
        list.emplace_back();
        if (!obj_addr(list.back())) {
            throw runtime_error("expected object address after ','");
            break;
        }
    } while (url.match(","));
}

bool UrlParser::obj_addr(ObjectAddress &addr) { return prot_addr(addr); }

bool UrlParser::prot_addr(ObjectAddress &addr) { return rir_prot_addr(addr) || iiop_prot_addr(addr); }

bool UrlParser::rir_prot_addr(ObjectAddress &addr) {
    if (url.match("rir:")) {
        addr.proto = "rir";
        return true;
    }
    return false;
}

bool UrlParser::iiop_prot_addr(ObjectAddress &addr) {
    if (!iiop_id()) {
        return false;
    }
    auto r = iiop_addr(addr);
    return r;
}

bool UrlParser::iiop_id() { return url.match("iiop:") || url.match(":"); }

bool UrlParser::iiop_addr(ObjectAddress &addr) {
    version(addr);
    host(addr);
    if (url.match(":")) {
        if (!port(addr)) {
            throw runtime_error("missing port number after :");
        }
    }
    return true;
}

bool UrlParser::version(ObjectAddress &addr) {
    auto start = url.pos;
    auto major = url.number();
    if (!major.has_value()) {
        return true;
    }
    if (!url.match(".")) {
        url.pos = start;
        return true;
    }
    auto minor = url.number();
    if (!minor.has_value()) {
        url.pos = start;
        return true;
    }
    if (!url.match("@")) {
        url.pos = start;
        return true;
    }
    addr.major = major.value();
    addr.minor = minor.value();
    return true;
}

// host, IPv4, IPv6
bool UrlParser::host(ObjectAddress &addr) {
    auto start = url.pos;

    if (url.match("[")) {
        auto end = url.data.find(']', start);
        if (end == string::npos) {
            throw runtime_error("missing ] in IPv6 address");
        }
        ++end;
        addr.host = url.data.substr(start, end - start);
        url.pos = end;
        return true;
    }

    do {
        label(addr);
    } while (url.match("."));
    if (start == url.pos) {
        return true;
    }
    addr.host = url.data.substr(start, url.pos - start);
    return true;
}

bool UrlParser::label(ObjectAddress &addr) {
    auto c = url.getc();
    if (!isalnum(c)) {
        url.ungetc();
        return true;
    }
    do {
        c = url.getc();
    } while (isalnum(c) || c == '-');
    url.ungetc();
    return true;
}

bool UrlParser::port(ObjectAddress &addr) {
    auto port = url.number();
    if (!port.has_value()) {
        return false;
    }
    addr.port = port.value();
    return true;
}

string UrlParser::key_string() {
    auto uri = url.uri();
    if (uri.has_value()) {
        return uri.value();
    }
    return "";
}

}  // namespace CORBA