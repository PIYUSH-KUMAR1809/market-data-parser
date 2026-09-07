#include "parsers/nse/NseFoParser.hpp"

#include <cstring>
#include <thread>
#include <vector>

#include "Endian.hpp"
#include "parsers/nse/NseFoProtocol.hpp"
#include "spdlog/spdlog.h"

namespace NseFo {

namespace {

struct RawOrder {
    int32_t token;
    int32_t price;
    int32_t quantity;
};

inline void fastFormatToken(int32_t token, char* buf) {
    uint64_t spaces = 0x2020202020202020ULL;
    std::memcpy(buf, &spaces, 8);
    char tmp[16];
    int i = 0;
    int32_t val = token;
    while (val > 0) {
        tmp[i++] = static_cast<char>('0' + (val % 10));
        val /= 10;
    }
    for (int j = 0; j < i; ++j) {
        buf[j] = tmp[i - 1 - j];
    }
}

void scanChunk(const char* buffer,
               size_t startOffset,
               size_t endOffset,
               std::vector<RawOrder>& outOrders) {
    size_t offset = startOffset;
    constexpr size_t recordSize = sizeof(SnapshotRecord);

    while (offset + recordSize <= endOffset) {
        // Fast skip of contiguous zero blocks (4 records = 80 bytes)
        while (offset + 80 <= endOffset &&
               *reinterpret_cast<const uint32_t*>(buffer + offset) == 0 &&
               *reinterpret_cast<const uint32_t*>(buffer + offset + 20) == 0 &&
               *reinterpret_cast<const uint32_t*>(buffer + offset + 40) == 0 &&
               *reinterpret_cast<const uint32_t*>(buffer + offset + 60) == 0) {
            offset += 80;
        }

        if (__builtin_expect(offset + recordSize > endOffset, 0)) {
            break;
        }

        const auto* record = reinterpret_cast<const SnapshotRecord*>(buffer + offset);
        offset += recordSize;

        if (*reinterpret_cast<const uint32_t*>(record) == 0) {
            continue;
        }

        int32_t token = MarketData::big_to_native(record->token);
        uint32_t u_token = static_cast<uint32_t>(token);

        if (__builtin_expect(u_token - 1 >= MAX_TOKEN_ID, 0)) {
            continue;
        }

        int32_t price = MarketData::big_to_native(record->price);
        int32_t quantity = MarketData::big_to_native(record->quantity);

        uint32_t u_price = static_cast<uint32_t>(price);
        uint32_t u_qty = static_cast<uint32_t>(quantity);

        if (__builtin_expect((u_price > MAX_PRICE_LIMIT) | (u_qty > MAX_QTY_LIMIT), 0)) {
            continue;
        }

        outOrders.push_back({token, price, quantity});
    }
}

}  // namespace

void NseFoParser::parseBufferParallel(const char* buffer, size_t size, size_t numThreads) {
    constexpr size_t recordSize = sizeof(SnapshotRecord);
    size_t totalRecords = size / recordSize;

    if (totalRecords == 0) return;

    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 8;
    }

    if (numThreads <= 1 || totalRecords < 100000) {
        std::vector<RawOrder> orders;
        orders.reserve(1024);
        scanChunk(buffer, 0, size, orders);
        uint64_t msgCount = 0;
        char symbolBuf[8];
        for (const auto& ord : orders) {
            auto* book = shardManager.getBook(ord.token);
            if (book) {
                msgCount++;
                fastFormatToken(ord.token, symbolBuf);
                book->addOrder(msgCount, 0, true, ord.price, ord.quantity, symbolBuf);
            }
        }
        return;
    }

    std::vector<std::vector<RawOrder>> threadOrders(numThreads);
    for (size_t i = 0; i < numThreads; ++i) {
        threadOrders[i].reserve(100000);
    }

    std::vector<std::thread> workers;
    workers.reserve(numThreads);

    size_t recordsPerThread = totalRecords / numThreads;

    auto t0 = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < numThreads; ++i) {
        size_t startRec = i * recordsPerThread;
        size_t endRec = (i == numThreads - 1) ? totalRecords : (i + 1) * recordsPerThread;

        size_t startOffset = startRec * recordSize;
        size_t endOffset = endRec * recordSize;

        workers.emplace_back([buffer, startOffset, endOffset, &threadOrders, i]() {
            scanChunk(buffer, startOffset, endOffset, threadOrders[i]);
        });
    }

    for (auto& w : workers) {
        if (w.joinable()) {
            w.join();
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();

    // Populate order books sequentially to preserve exact message sequence numbers
    uint64_t msgCount = 0;
    char symbolBuf[8];
    for (size_t i = 0; i < numThreads; ++i) {
        for (const auto& ord : threadOrders[i]) {
            auto* book = shardManager.getBook(ord.token);
            if (book) {
                msgCount++;
                fastFormatToken(ord.token, symbolBuf);
                book->addOrder(msgCount, 0, true, ord.price, ord.quantity, symbolBuf);
            }
        }
    }

    auto t2 = std::chrono::high_resolution_clock::now();

    spdlog::debug("Parallel parse ({} threads): Scan = {:.3f}s, Book Population = {:.3f}s",
                  numThreads,
                  std::chrono::duration<double>(t1 - t0).count(),
                  std::chrono::duration<double>(t2 - t1).count());
}

void NseFoParser::parseBuffer(const char* buffer, size_t size) {
    if (size >= 10 * 1024 * 1024) {
        parseBufferParallel(buffer, size);
    } else {
        std::vector<RawOrder> orders;
        scanChunk(buffer, 0, size, orders);
        uint64_t msgCount = 0;
        char symbolBuf[8];
        for (const auto& ord : orders) {
            auto* book = shardManager.getBook(ord.token);
            if (book) {
                msgCount++;
                fastFormatToken(ord.token, symbolBuf);
                book->addOrder(msgCount, 0, true, ord.price, ord.quantity, symbolBuf);
            }
        }
    }
}

void NseFoParser::parseBlock(const char* block) {
    (void)block;
}

}  // namespace NseFo
