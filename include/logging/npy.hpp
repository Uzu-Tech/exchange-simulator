#pragma once

#include <cstdint>
#include <string_view>
#include <string>
#include <format>
#include <unistd.h>

#include "settings.hpp"

// ── Npy type trait ────────────────────────────────────────────────────────────
template<typename T> struct NpyType;
template<> struct NpyType<float>    { static constexpr std::string_view code = "<f4"; };
template<> struct NpyType<double>   { static constexpr std::string_view code = "<f8"; };
template<> struct NpyType<int32_t>  { static constexpr std::string_view code = "<i4"; };
template<> struct NpyType<int64_t>  { static constexpr std::string_view code = "<i8"; };
template<> struct NpyType<uint32_t> { static constexpr std::string_view code = "<u4"; };
template<> struct NpyType<uint64_t> { static constexpr std::string_view code = "<u8"; };
template<> struct NpyType<int8_t>   { static constexpr std::string_view code = "|i1"; };
template<> struct NpyType<uint8_t>  { static constexpr std::string_view code = "|u1"; };

// ── Npy header writer ─────────────────────────────────────────────────────────
namespace NpyHeader {
template<typename Log>
std::string make_dtype_str() {
    std::string s = "[";
    bool first = true;

    std::apply([&](auto&&... field) {
        ([&] {
            if (!first) s += ", ";
            s += std::format("('{}', '{}')",
                             field.first,
                             field.second);
            first = false;
        }(), ...);
    }, Log::npy_fields());

    s += "]";
    return s;
}

template<typename Log>
std::string make_header_str(size_t num_records) {
    return std::format(
        "{{'descr': {}, 'fortran_order': False, 'shape': ({},), }}",
        make_dtype_str<Log>(),
        num_records
    );
}

template<typename Log>
size_t header_byte_size() {
    // Use max plausible shape to get a stable size
    std::string header = make_header_str<Log>(settings::MAX_NUM_LOGS);
    size_t pre   = 6 + 2 + 2;  // magic + version + header_len
    size_t total = pre + header.size() + 1;  // +1 for \n terminator
    return ((total + 63) / 64) * 64;
}

template<typename Log>
void write_header(int fd, size_t num_records, size_t buf_size) {
    std::string header = make_header_str<Log>(num_records);
 
    size_t pre = 6 + 2 + 2; // magic + version + header len
    size_t total = pre + header.size() + 1;
    size_t padding = buf_size - total;
 
    header += std::string(padding, ' ');
    header += '\n';
 
    uint16_t header_len = static_cast<uint16_t>(header.size());
 
    lseek(fd, 0, SEEK_SET);
    ::write(fd, "\x93NUMPY", 6); // Magic 6 bytes that tells you it's a numpy file
    uint8_t version[2] = {1, 0}; // Version of numpy parsing rules 1.0
    ::write(fd, version,  2);
    ::write(fd, &header_len, 2); // Length of header string as 2 bytes
    ::write(fd, header.data(), header.size()); // Description string
}
}
