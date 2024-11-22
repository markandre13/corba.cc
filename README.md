# corba.cc

This is an ORB for [corba.js](https://github.com/markandre13/corba.js) in Modern
C++ (mostly) utilizing std::shared_ptr, std::string, std::string_view and
coroutines.

## macOS

    brew install llvm libev wslay nettle

## Debian

Debian testing/trixie is needed and

    apt-get install clang-19 libev-dev libwslay-dev nettle-dev uuid-dev
