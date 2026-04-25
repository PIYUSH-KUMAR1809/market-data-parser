#include <gtest/gtest.h>

#include <vector>

#include "Endian.hpp"
#include "parsers/nse/NseFoParser.hpp"

using namespace NseFo;

void writeBE32(std::vector<char>& buffer, int32_t val) {
    uint32_t uval = static_cast<uint32_t>(val);
    uint32_t swapped = MarketData::big_to_native(uval);

    const char* ptr = reinterpret_cast<const char*>(&swapped);
    buffer.insert(buffer.end(), ptr, ptr + 4);
}

TEST(NseParserTest, ParseSingleRecord) {
    NseFoParser parser;
    std::vector<char> buffer;

    int32_t token = 1001;
    int32_t price = 50000;
    int32_t qty = 100;

    writeBE32(buffer, token);
    writeBE32(buffer, price);
    writeBE32(buffer, qty);
    writeBE32(buffer, 0);
    writeBE32(buffer, 0);

    parser.parseBuffer(buffer.data(), buffer.size());

    EXPECT_EQ(parser.getTotalOrderCount(), 1);
}

TEST(NseParserTest, IgnoreInvalidToken) {
    NseFoParser parser;
    std::vector<char> buffer;

    writeBE32(buffer, -1);
    writeBE32(buffer, 500);
    writeBE32(buffer, 10);
    writeBE32(buffer, 0);
    writeBE32(buffer, 0);

    parser.parseBuffer(buffer.data(), buffer.size());
    EXPECT_EQ(parser.getTotalOrderCount(), 0);
}
