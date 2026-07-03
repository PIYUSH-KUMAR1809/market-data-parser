#pragma once

#include <cstdint>
#include <cstring>

namespace MarketData {

using OrderId = uint64_t;
using Price = int64_t;
using Quantity = uint32_t;

struct Order {
    OrderId id = 0;
    uint64_t timestamp = 0;
    bool isBuy = false;
    Price price = 0;
    Quantity quantity = 0;
    char symbol[8];

    Order() { std::memset(symbol, 0, 8); }
};

struct PriceLevel {
    Price price = 0;
    uint64_t totalQuantity = 0;
    uint32_t orderCount = 0;
};

}
