#pragma once

#include <array>
#include <cctype>
#include <cstdint>

#include "Endian.hpp"
#include "ItchMessages.hpp"
#include "OrderBook.hpp"
#include "spdlog/spdlog.h"

namespace MarketData {

class ItchParser {
   public:
    ItchParser() { book_.reserveOrders(1 << 21); }

    size_t messagesParsed = 0;
    size_t totalBytesParsed = 0;
    size_t unhandledMessages = 0;

    std::array<size_t, 256> msgCounts{};

    uint16_t filterMinLocate_ = 0;
    uint16_t filterMaxLocate_ = 65535;

    void setFilterRange(uint16_t minLocate, uint16_t maxLocate) {
        filterMinLocate_ = minLocate;
        filterMaxLocate_ = maxLocate;
    }

    size_t getTotalOrderCount() const { return book_.getOrderCount(); }

    bool verifyComplete(size_t fileSize) const {
        if (totalBytesParsed != fileSize) {
            spdlog::error("[ERROR] Parser did not consume entire file! Parsed {} / {} bytes.",
                          totalBytesParsed,
                          fileSize);
            return false;
        }
        return true;
    }

    void parseBuffer(const char* buffer, size_t size) {
        size_t offset = 0;
        const bool filtered = (filterMinLocate_ != 0 || filterMaxLocate_ != 65535);

        while (offset + 2 <= size) {
            const uint16_t msgLen = load_be16(buffer + offset);
            offset += 2;
            if (__builtin_expect(offset + msgLen > size, 0)) {
                break;
            }

            const char* msgPtr = buffer + offset;
            const uint8_t msgType = static_cast<uint8_t>(msgPtr[0]);

            if (filtered) {
                uint16_t locate = 0;
                if (msgLen >= 3) {
                    locate = load_be16(msgPtr + 1);
                }
                if (locate < filterMinLocate_ || locate > filterMaxLocate_) {
                    offset += msgLen;
                    continue;
                }
            }

            msgCounts[msgType]++;
            dispatch(msgType, msgPtr);
            offset += msgLen;
            messagesParsed++;
        }
        totalBytesParsed = offset;
    }

    void mergeFrom(ItchParser& other) {
        messagesParsed += other.messagesParsed;
        unhandledMessages += other.unhandledMessages;
        if (other.totalBytesParsed > totalBytesParsed) {
            totalBytesParsed = other.totalBytesParsed;
        }
        for (size_t i = 0; i < 256; ++i) {
            msgCounts[i] += other.msgCounts[i];
        }
        book_.mergeFrom(other.book_);
    }

    void printStats() const {
        spdlog::info("\nMessage Statistics:");
        spdlog::info("--------------------------------");
        spdlog::info("  Type |          Count");
        spdlog::info("--------------------------------");
        for (int i = 0; i < 256; ++i) {
            if (msgCounts[i] > 0) {
                if (isprint(i) != 0) {
                    spdlog::info("   {}   | {:14}", static_cast<char>(i), msgCounts[i]);
                } else {
                    spdlog::info(" {:#x}  | {:14}", i, msgCounts[i]);
                }
            }
        }
        spdlog::info("--------------------------------");
    }

    bool printDebug = false;

   private:
    OrderBook book_;

    inline void dispatch(uint8_t msgType, const char* ptr) {
        switch (msgType) {
            case 'A': handleAddOrder(ptr); break;
            case 'F': handleAddOrderMPID(ptr); break;
            case 'E': handleOrderExecuted(ptr); break;
            case 'C': handleOrderExecutedWithPrice(ptr); break;
            case 'X': handleOrderCancel(ptr); break;
            case 'D': handleOrderDelete(ptr); break;
            case 'U': handleOrderReplace(ptr); break;
            case 'S':
            case 'R':
            case 'H':
            case 'Y':
            case 'L':
            case 'V':
            case 'W':
            case 'K':
            case 'J':
            case 'h':
            case 'P':
            case 'Q':
            case 'B':
            case 'I':
            case 'N':
                break;
            default:
                ++unhandledMessages;
                break;
        }
    }

    void handleAddOrder(const char* ptr) {
        book_.addOrder(load_be64(ptr + 11),
                       ptr[19] == 'B',
                       load_be32(ptr + 32),
                       load_be32(ptr + 20),
                       ptr + 24);
    }

    void handleAddOrderMPID(const char* ptr) {
        book_.addOrder(load_be64(ptr + 11),
                       ptr[19] == 'B',
                       load_be32(ptr + 32),
                       load_be32(ptr + 20),
                       ptr + 24);
    }

    void handleOrderExecuted(const char* ptr) {
        book_.executeOrder(load_be64(ptr + 11), load_be32(ptr + 19));
    }

    void handleOrderExecutedWithPrice(const char* ptr) {
        book_.executeOrder(load_be64(ptr + 11), load_be32(ptr + 19));
    }

    void handleOrderDelete(const char* ptr) { book_.deleteOrder(load_be64(ptr + 11)); }

    void handleOrderReplace(const char* ptr) {
        book_.replaceOrder(load_be64(ptr + 11),
                           load_be64(ptr + 19),
                           load_be32(ptr + 31),
                           load_be32(ptr + 27));
    }

    void handleOrderCancel(const char* ptr) {
        book_.cancelOrder(load_be64(ptr + 11), load_be32(ptr + 19));
    }
};

}
