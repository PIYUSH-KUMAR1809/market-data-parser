#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "Endian.hpp"
#include "ItchParser.hpp"

using namespace MarketData;

namespace {

template <typename T>
void writeBE(std::vector<char>& buf, T val) {
    T swapped = big_to_native(val);
    const char* p = reinterpret_cast<const char*>(&swapped);
    buf.insert(buf.end(), p, p + sizeof(T));
}

void writeStock(std::vector<char>& buf, const char* stock) {
    char padded[8];
    std::memset(padded, ' ', 8);
    std::size_t n = std::strlen(stock);
    if (n > 8) n = 8;
    std::memcpy(padded, stock, n);
    buf.insert(buf.end(), padded, padded + 8);
}

void finishMessage(std::vector<char>& out, const std::vector<char>& payload) {
    writeBE<uint16_t>(out, static_cast<uint16_t>(payload.size()));
    out.insert(out.end(), payload.begin(), payload.end());
}

void appendAddOrder(std::vector<char>& out,
                    uint16_t locate,
                    uint64_t orderId,
                    char side,
                    uint32_t shares,
                    const char* stock,
                    uint32_t price) {
    std::vector<char> msg;
    msg.push_back('A');
    writeBE<uint16_t>(msg, locate);
    writeBE<uint16_t>(msg, 0);
    msg.insert(msg.end(), 6, '\0');
    writeBE<uint64_t>(msg, orderId);
    msg.push_back(side);
    writeBE<uint32_t>(msg, shares);
    writeStock(msg, stock);
    writeBE<uint32_t>(msg, price);
    finishMessage(out, msg);
}

void appendDelete(std::vector<char>& out, uint16_t locate, uint64_t orderId) {
    std::vector<char> msg;
    msg.push_back('D');
    writeBE<uint16_t>(msg, locate);
    writeBE<uint16_t>(msg, 0);
    msg.insert(msg.end(), 6, '\0');
    writeBE<uint64_t>(msg, orderId);
    finishMessage(out, msg);
}

void appendStockTradingAction(std::vector<char>& out, uint16_t locate, const char* stock) {
    std::vector<char> msg;
    msg.push_back('H');
    writeBE<uint16_t>(msg, locate);
    writeBE<uint16_t>(msg, 0);
    msg.insert(msg.end(), 6, '\0');
    writeStock(msg, stock);
    msg.push_back('T');
    msg.push_back('\0');
    msg.insert(msg.end(), 4, ' ');
    finishMessage(out, msg);
}

void appendSystemEvent(std::vector<char>& out, char code) {
    std::vector<char> msg;
    msg.push_back('S');
    writeBE<uint16_t>(msg, 0);
    writeBE<uint16_t>(msg, 0);
    msg.insert(msg.end(), 6, '\0');
    msg.push_back(code);
    finishMessage(out, msg);
}

}

TEST(ItchParser, AddThenDeleteLeavesEmptyBook) {
    std::vector<char> buf;
    appendAddOrder(buf, 42, 1001, 'B', 200, "AAPL", 1500000);
    appendDelete(buf, 42, 1001);

    ItchParser parser;
    parser.parseBuffer(buf.data(), buf.size());

    EXPECT_EQ(parser.messagesParsed, 2u);
    EXPECT_EQ(parser.totalBytesParsed, buf.size());
    EXPECT_EQ(parser.getTotalOrderCount(), 0u);
    EXPECT_EQ(parser.msgCounts['A'], 1u);
    EXPECT_EQ(parser.msgCounts['D'], 1u);
    EXPECT_EQ(parser.unhandledMessages, 0u);
}

TEST(ItchParser, AddOrderStaysOnBook) {
    std::vector<char> buf;
    appendAddOrder(buf, 7, 99, 'S', 50, "MSFT", 4200000);

    ItchParser parser;
    parser.parseBuffer(buf.data(), buf.size());

    EXPECT_EQ(parser.messagesParsed, 1u);
    EXPECT_EQ(parser.getTotalOrderCount(), 1u);
}

TEST(ItchParser, AdminMessagesDoNotCreateOrders) {
    std::vector<char> buf;
    appendSystemEvent(buf, 'O');
    appendStockTradingAction(buf, 10, "AAPL");
    appendAddOrder(buf, 10, 1, 'B', 10, "AAPL", 10000);

    ItchParser parser;
    parser.parseBuffer(buf.data(), buf.size());

    EXPECT_EQ(parser.messagesParsed, 3u);
    EXPECT_EQ(parser.msgCounts['S'], 1u);
    EXPECT_EQ(parser.msgCounts['H'], 1u);
    EXPECT_EQ(parser.msgCounts['A'], 1u);
    EXPECT_EQ(parser.getTotalOrderCount(), 1u);
    EXPECT_EQ(parser.unhandledMessages, 0u);
}

TEST(ItchParser, LocateFilterIgnoresOtherSymbols) {
    std::vector<char> buf;
    appendAddOrder(buf, 1, 1, 'B', 10, "AAA", 100);
    appendAddOrder(buf, 50000, 2, 'B', 10, "ZZZ", 100);

    ItchParser parser;
    parser.setFilterRange(0, 100);
    parser.parseBuffer(buf.data(), buf.size());

    EXPECT_EQ(parser.messagesParsed, 1u);
    EXPECT_EQ(parser.msgCounts['A'], 1u);
    EXPECT_EQ(parser.getTotalOrderCount(), 1u);
    EXPECT_EQ(parser.totalBytesParsed, buf.size());
}

TEST(ItchParser, LocatePartitionMergeCountsEachMessageOnce) {
    std::vector<char> buf;
    appendAddOrder(buf, 1, 1, 'B', 10, "AAA", 100);
    appendAddOrder(buf, 50000, 2, 'S', 20, "ZZZ", 200);
    appendStockTradingAction(buf, 1, "AAA");

    ItchParser low;
    low.setFilterRange(0, 32767);
    low.parseBuffer(buf.data(), buf.size());

    ItchParser high;
    high.setFilterRange(32768, 65535);
    high.parseBuffer(buf.data(), buf.size());

    ItchParser merged;
    merged.mergeFrom(low);
    merged.mergeFrom(high);

    EXPECT_EQ(low.messagesParsed, 2u);
    EXPECT_EQ(high.messagesParsed, 1u);
    EXPECT_EQ(merged.messagesParsed, 3u);
    EXPECT_EQ(merged.msgCounts['A'], 2u);
    EXPECT_EQ(merged.msgCounts['H'], 1u);
    EXPECT_EQ(merged.getTotalOrderCount(), 2u);
    EXPECT_EQ(merged.totalBytesParsed, buf.size());
}

TEST(ItchParser, ConsumesEntireBuffer) {
    std::vector<char> buf;
    appendSystemEvent(buf, 'S');
    appendAddOrder(buf, 3, 9, 'B', 1, "IBM", 1);

    ItchParser parser;
    parser.parseBuffer(buf.data(), buf.size());
    EXPECT_TRUE(parser.verifyComplete(buf.size()));
}
