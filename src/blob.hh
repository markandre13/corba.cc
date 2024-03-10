#pragma once

#include <format>
#include <iomanip>
#include <string>
#include <string_view>
#include <sstream>

namespace CORBA {

namespace detail {

template <typename Char>
struct basic_ostream_formatter : std::formatter<std::basic_string_view<Char>, Char> {
        template <typename T, typename OutputIt>
        auto format(const T &value, std::basic_format_context<OutputIt, Char> &ctx) const -> OutputIt {
            std::basic_stringstream<Char> ss;
            ss << value;
            return std::formatter<std::basic_string_view<Char>, Char>::format(ss.view(), ctx);
        }
};
using ostream_formatter = basic_ostream_formatter<char>;

}  // namespace detail

class blob;

class blob_view : public std::basic_string_view<std::byte> {
    private:
        using base_t = std::basic_string_view<std::byte>;

    public:
        inline blob_view(): base_t() {}
        inline blob_view(const char *buffer) : base_t(reinterpret_cast<const std::byte *>(buffer), strlen(buffer)) {}
        inline blob_view(const void *buffer, size_t nbytes) : base_t(reinterpret_cast<const std::byte *>(buffer), nbytes) {}
        inline blob_view(const std::string &blob): base_t(reinterpret_cast<const std::byte *>(blob.data()), blob.size()) {}
        inline blob_view(const blob &blob);
        // friend ostream &operator<<(ostream &os, const blob_view &dt);
};

class blob : public std::basic_string<std::byte> {
    private:
        using base_t = std::basic_string<std::byte>;

    public:
        inline blob(): base_t() {}
        inline blob(const char *buffer) : base_t(reinterpret_cast<const std::byte *>(buffer), strlen(buffer)) {}
        inline blob(const void *buffer, size_t nbytes) : base_t(reinterpret_cast<const std::byte *>(buffer), nbytes) {}
        inline blob(const std::string &buffer): base_t(reinterpret_cast<const std::byte *>(buffer.data()), buffer.size()) {}
        inline blob(const blob_view &value) : base_t(value) {}
        // operator blob_view() const { return blob_view(*this);}

        // friend ostream &operator<<(ostream &os, const blob_view &dt);
};

inline blob_view::blob_view(const blob &blob): base_t(reinterpret_cast<const std::byte *>(blob.data()), blob.size()) {}

inline std::ostream &operator<<(std::ostream &os, const blob &dt) {
    os << std::hex << std::setfill('0');
    for (auto &b : dt) {
        os << std::setw(2) << (unsigned)b;
    }
    return os;
}

inline std::ostream &operator<<(std::ostream &os, const blob_view &dt) {
    os << std::hex << std::setfill('0');
    for (auto &b : dt) {
        os << std::setw(2) << (unsigned)b;
    }
    return os;
}

}  // namespace CORBA

template <>
struct std::formatter<CORBA::blob_view> : CORBA::detail::ostream_formatter {};

template <>
struct std::formatter<CORBA::blob> : CORBA::detail::ostream_formatter {};
