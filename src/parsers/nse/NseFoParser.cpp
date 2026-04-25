#include "parsers/nse/NseFoParser.hpp"

#include <charconv>
#include <cstring>

#include "Endian.hpp"
#include "parsers/nse/NseFoProtocol.hpp"
#include "spdlog/spdlog.h"

namespace NseFo {

void NseFoParser::parseBuffer(const char* buffer, size_t size) {
    size_t offset = 0;
    size_t recordSize = sizeof(SnapshotRecord);
    uint64_t msgCount = 0;

    char symbolBuf[8];

    while (offset + recordSize <= size) {
        const auto* record = reinterpret_cast<const SnapshotRecord*>(buffer + offset);
        offset += recordSize;

        int32_t token = MarketData::big_to_native(record->token);

        if (token <= 0 || token > MAX_TOKEN_ID) {
            spdlog::trace("Skipping invalid token: {}", token);
            continue;
        }

        int32_t price = MarketData::big_to_native(record->price);
        int32_t quantity = MarketData::big_to_native(record->quantity);

        if (price > MAX_PRICE_LIMIT || price < 0) {
            spdlog::trace("Skipping invalid price: {}", price);
            continue;
        }

        if (quantity > MAX_QTY_LIMIT || quantity < 0) {
            spdlog::trace("Skipping invalid quantity: {}", quantity);
            continue;
        }

        MarketData::OrderBook* book = shardManager.getBook(token);
        if (book != nullptr) {
            msgCount++;

            std::memset(symbolBuf, ' ', 8);
            auto [ptr, ec] = std::to_chars(symbolBuf, symbolBuf + 8, token);
            (void)ptr;
            (void)ec;

            book->addOrder(msgCount,
                           0,
                           true,
                           static_cast<int64_t>(price),
                           static_cast<uint32_t>(quantity),
                           symbolBuf);
        }
    }
}

void NseFoParser::parseBlock(const char* block) {
    (void)block;
}

}  // namespace NseFo
