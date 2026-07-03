#pragma once

#include <bit>

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

}
