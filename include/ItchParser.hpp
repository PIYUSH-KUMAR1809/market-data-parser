#pragma once

#include <array>
#include <cctype>
#include <cstdint>

#include "Endian.hpp"
#include "ItchMessages.hpp"
#include "OrderBook.hpp"
#include "ShardManager.hpp"
#include "spdlog/spdlog.h"

namespace MarketData {

class ItchParser {
   public:
    static constexpr size_t kLocateCount = 65536;

    ItchParser() : shardManager(32 * 1024 * 1024, kLocateCount) {}

    size_t messagesParsed = 0;
    size_t totalBytesParsed = 0;
    size_t unhandledMessages = 0;

    std::array<size_t, 256> msgCounts{};
    ShardManager shardManager;

    uint16_t filterMinLocate_ = 0;
    uint16_t filterMaxLocate_ = 65535;

    void setFilterRange(uint16_t minLocate, uint16_t maxLocate) {
        filterMinLocate_ = minLocate;
        filterMaxLocate_ = maxLocate;
    }

    size_t getTotalOrderCount() const { return shardManager.getTotalOrderCount(); }

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
            const uint16_t msgLen =
                big_to_native<uint16_t>(*reinterpret_cast<const uint16_t*>(buffer + offset));
            offset += 2;
            if (__builtin_expect(offset + msgLen > size, 0)) {
                break;
            }

            const char* msgPtr = buffer + offset;
            const uint8_t msgType = static_cast<uint8_t>(msgPtr[0]);

            if (filtered) {
                uint16_t locate = 0;
                if (msgLen >= 3) {
                    locate = big_to_native<uint16_t>(*reinterpret_cast<const uint16_t*>(msgPtr + 1));
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
        shardManager.mergeFrom(other.shardManager);
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
    inline OrderBook* getBook(const char* ptr) {
        const uint16_t stockLocate =
            big_to_native<uint16_t>(*reinterpret_cast<const uint16_t*>(ptr + 1));
        return shardManager.getBook(stockLocate);
    }

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
        const auto* msg = reinterpret_cast<const AddOrderMsg*>(ptr);
        getBook(ptr)->addOrder(big_to_native(msg->orderReferenceNumber),
                               msg->buySellIndicator == 'B',
                               big_to_native(msg->price),
                               big_to_native(msg->shares),
                               msg->stock);
    }

    void handleAddOrderMPID(const char* ptr) {
        const auto* msg = reinterpret_cast<const AddOrderMPIDMsg*>(ptr);
        getBook(ptr)->addOrder(big_to_native(msg->orderReferenceNumber),
                               msg->buySellIndicator == 'B',
                               big_to_native(msg->price),
                               big_to_native(msg->shares),
                               msg->stock);
    }

    void handleOrderExecuted(const char* ptr) {
        const auto* msg = reinterpret_cast<const OrderExecutedMsg*>(ptr);
        getBook(ptr)->executeOrder(big_to_native(msg->orderReferenceNumber),
                                   big_to_native(msg->executedShares));
    }

    void handleOrderExecutedWithPrice(const char* ptr) {
        const auto* msg = reinterpret_cast<const OrderExecutedWithPriceMsg*>(ptr);
        getBook(ptr)->executeOrder(big_to_native(msg->orderReferenceNumber),
                                   big_to_native(msg->executedShares));
    }

    void handleOrderDelete(const char* ptr) {
        const auto* msg = reinterpret_cast<const OrderDeleteMsg*>(ptr);
        getBook(ptr)->deleteOrder(big_to_native(msg->orderReferenceNumber));
    }

    void handleOrderReplace(const char* ptr) {
        const auto* msg = reinterpret_cast<const OrderReplaceMsg*>(ptr);
        getBook(ptr)->replaceOrder(big_to_native(msg->originalOrderReferenceNumber),
                                   big_to_native(msg->newOrderReferenceNumber),
                                   big_to_native(msg->price),
                                   big_to_native(msg->shares));
    }

    void handleOrderCancel(const char* ptr) {
        const auto* msg = reinterpret_cast<const OrderCancelMsg*>(ptr);
        getBook(ptr)->cancelOrder(big_to_native(msg->orderReferenceNumber),
                                  big_to_native(msg->canceledShares));
    }
};

}
