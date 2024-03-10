#include "util.hh"

using namespace std;

void sendChordata(void *, unsigned long) {}

// due to
//
//   CP.51: Do not use capturing lambdas that are coroutines.
//   https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rcoro-capture
//
// the following causes a memory error when accessing 'orb' after the co_await
//
//   [&orb] -> async<> { ... co_await ... }().thenOrCatch(...)
//
// as the closure object will be destroyed after the co_await.
//
// this on the other hand works with clang:
//
//   auto call(std::function<async<>()> closure) { return closure(); }
//   call([&orb] -> async<> { ... co_await ... }).thenOrCatch(...)
//
// and i assume it's because then the compiler has to pass the closure forward
// into the call() function and then it's lifetime get's intertwined with the
// coroutine.
void parallel(std::exception_ptr &eptr, std::function<CORBA::async<>()> closure) {
    closure().thenOrCatch([] {},
                          [&eptr](std::exception_ptr _eptr) {
                              eptr = _eptr;
                          });
}
void parallel(std::exception_ptr &eptr, struct ev_loop *loop, std::function<CORBA::async<>()> closure) {
    closure().thenOrCatch(
        [loop] {
            ev_break(loop);
        },
        [&eptr, loop](std::exception_ptr _eptr) {
            eptr = _eptr;
            ev_break(loop);
        });
}

vector<uint8_t> parseOmniDump(const string_view &data) {
    vector<uint8_t> result;
    auto lines = split(trim(data));
    string hex("00");
    for (auto row : lines) {
        row = trim(row);
        for (auto i = 0; i < 8; ++i) {
            unsigned idx = i * 5;
            hex[0] = row[idx];
            hex[1] = row[idx + 1];
            if (isspace(hex[0])) {
                break;
            }
            result.push_back(stoi(hex, nullptr, 16));
            hex[0] = row[idx + 2];
            hex[1] = row[idx + 3];
            result.push_back(stoi(hex, nullptr, 16));
        }
    }
    return result;
}

vector<string_view> split(string_view data) {
    vector<string_view> result;
    size_t bol = 0;
    while (true) {
        size_t eol = data.find('\n', bol);
        if (eol == string::npos) {
            result.push_back(data.substr(bol));
            break;
        }
        result.push_back(data.substr(bol, eol - bol));
        bol = eol + 1;
    }
    return result;
}

string_view trim(string_view data) {
    size_t bol = 0;
    while (bol < data.length()) {
        if (!isspace(data[bol])) {
            break;
        }
        ++bol;
    }
    size_t eol = data.length();
    if (bol == eol) {
        return string_view();
    }
    while (eol > 0) {
        --eol;
        if (!isspace(data[eol])) {
            break;
        }
    }
    return data.substr(bol, eol);
}
