#pragma once

#include <bit>
#include <cstdint>
#include <cstring>

namespace MarketData {

template <typename T>
inline T big_to_native(T value) {
    if constexpr (std::endian::native == std::endian::big) {
        return value;
    } else {
        if constexpr (sizeof(T) == 2)
            return __builtin_bswap16(value);
        else if constexpr (sizeof(T) == 4)
            return __builtin_bswap32(value);
        else if constexpr (sizeof(T) == 8)
            return __builtin_bswap64(value);
        else
            return value;
    }
}

template <typename T>
inline T load_be(const void* p) {
    T v{};
    std::memcpy(&v, p, sizeof(T));
    return big_to_native(v);
}

inline uint16_t load_be16(const void* p) { return load_be<uint16_t>(p); }
inline uint32_t load_be32(const void* p) { return load_be<uint32_t>(p); }
inline uint64_t load_be64(const void* p) { return load_be<uint64_t>(p); }

}
