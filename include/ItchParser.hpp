#pragma once

#include <array>
#include <string>

#include "Endian.hpp"
#include "Histogram.hpp"
#include "ItchMessages.hpp"
#include "OrderBook.hpp"
#include "ShardManager.hpp"

namespace MarketData {

class ItchParser {
   public:
    ItchParser() = default;

    size_t messagesParsed = 0;
    size_t totalBytesParsed = 0;
    size_t unhandledMessages = 0;

    std::array<size_t, 256> msgCounts{};
    Histogram latencyHist;
    uint16_t lastTrackingNumber = 0;
    size_t totalGaps = 0;

    ShardManager shardManager;

    size_t getTotalOrderCount() const { return shardManager.getTotalOrderCount(); }

    bool verifyComplete(size_t fileSize) const {
        bool success = true;
        if (totalBytesParsed != fileSize) {
            spdlog::error("[ERROR] Parser did not consume entire file! Parsed {} / {} bytes.",
                          totalBytesParsed,
                          fileSize);
            success = false;
        }
        return success;
    }

    void parseBuffer(const char *buffer, size_t size) {
        size_t offset = 0;

        while (offset + 2 <= size) {
            auto t0 = std::chrono::steady_clock::now();
            uint16_t msgLen = big_to_native<uint16_t>(*(const uint16_t *)(buffer + offset));
            offset += 2;

            if (offset + msgLen > size) {
                break;
            }

            const char *msgPtr = buffer + offset;
            uint8_t msgType = static_cast<uint8_t>(msgPtr[0]);

            msgCounts[msgType]++;

            if (msgLen >= 5) {
                uint16_t tracking = big_to_native<uint16_t>(*(const uint16_t *)(msgPtr + 3));
                checkGap(tracking);
            }

            switch (msgType) {
                case 'S':
                    handleSystemEvent(msgPtr);
                    break;
                case 'R':
                    handleStockDirectory(msgPtr);
                    break;
                case 'H':
                case 'Y':
                case 'L':
                case 'V':
                case 'W':
                case 'K':
                case 'J':
                case 'h':
                case 'A':
                    handleAddOrder(msgPtr);
                    break;
                case 'F':
                    handleAddOrderMPID(msgPtr);
                    break;
                case 'E':
                    handleOrderExecuted(msgPtr);
                    break;
                case 'C':
                    handleOrderExecutedWithPrice(msgPtr);
                    break;
                case 'X':
                    handleOrderCancel(msgPtr);
                    break;
                case 'D':
                    handleOrderDelete(msgPtr);
                    break;
                case 'U':
                    handleOrderReplace(msgPtr);
                    break;
                case 'P':
                    handleTrade(msgPtr);
                    break;
                case 'Q':
                    handleCrossTrade(msgPtr);
                    break;
                case 'B':
                    handleBrokenTrade(msgPtr);
                    break;
                case 'I':
                    handleNOII(msgPtr);
                    break;
                case 'N':
                    break;
                default:
                    unhandledMessages++;
                    break;
            }

            offset += msgLen;
            messagesParsed++;

            auto t1 = std::chrono::steady_clock::now();
            uint64_t nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
            latencyHist.record(nanos);
        }
        totalBytesParsed = offset;
    }

    void printStats() const {
        spdlog::info("\nMessage Statistics:");
        spdlog::info("--------------------------------");
        spdlog::info("  Type |          Count");
        spdlog::info("--------------------------------");
        for (int i = 0; i < 256; ++i) {
            if (msgCounts[i] > 0) {
                if (isprint(i) != 0) {
                    spdlog::info("   {}   | {:14}", (char)i, msgCounts[i]);
                } else {
                    spdlog::info(" {:#x}  | {:14}", i, msgCounts[i]);
                }
            }
        }
        spdlog::info("--------------------------------");

        spdlog::info("--------------------------------");

        latencyHist.printStats();

        if (totalGaps > 0) {
            spdlog::warn("WARNING: Detected {} gaps in sequence numbers.", totalGaps);
        } else {
            spdlog::info("SUCCESS: No sequence number gaps detected.");
        }
    }

    bool printDebug = false;

   private:
    static double formatPrice(uint32_t price) { return big_to_native(price) / 10000.0; }

    static std::string formatStock(const char *stock) { return std::string(stock, 8); }

    void log(const std::string &msg) const {
        if (printDebug && messagesParsed < 10) {
            spdlog::debug("[MSG {}] {}", messagesParsed, msg);
        }
    }

    void checkGap(uint16_t currentTracking) {
        if (messagesParsed == 0) {
            lastTrackingNumber = currentTracking;
            return;
        }

        uint16_t expected = lastTrackingNumber + 1;
        if (currentTracking != expected) {
            totalGaps++;
        }
        lastTrackingNumber = currentTracking;
    }

    OrderBook *getBookForMsg(const char *ptr) {
        const auto *header = reinterpret_cast<const ItchHeader *>(ptr);
        uint16_t loc = big_to_native(header->stockLocate);
        return shardManager.getBook(loc);
    }

    void handleSystemEvent(const char *ptr) {
        const auto *msg = reinterpret_cast<const SystemEventMsg *>(ptr);
        if (printDebug && messagesParsed < 10) {
            uint16_t tracking = big_to_native(msg->header.trackingNumber);
            log("System Event: Code=" + std::string(1, msg->eventCode) +
                ", Tracking=" + std::to_string(tracking));
        }
    }

    void handleAddOrder(const char *ptr) {
        const auto *msg = reinterpret_cast<const AddOrderMsg *>(ptr);

        uint64_t orderId = big_to_native(msg->orderReferenceNumber);
        uint32_t price = big_to_native(msg->price);
        uint32_t shares = big_to_native(msg->shares);
        bool isBuy = (msg->buySellIndicator == 'B');
        uint64_t timestamp = msg->header.getTimestamp();

        OrderBook *book = getBookForMsg(ptr);
        if (book != nullptr) {
            book->addOrder(orderId, timestamp, isBuy, price, shares, msg->stock);
        }

        if (printDebug && messagesParsed < 10) {
            std::string stock = formatStock(msg->stock);
            double displayPrice = formatPrice(msg->price);
            log("AddOrder: " + stock + " " + (isBuy ? "B" : "S") + " " + std::to_string(shares) +
                " @ " + std::to_string(displayPrice));
        }
    }

    void handleAddOrderMPID(const char *ptr) {
        const auto *msg = reinterpret_cast<const AddOrderMPIDMsg *>(ptr);

        uint64_t orderId = big_to_native(msg->orderReferenceNumber);
        uint32_t price = big_to_native(msg->price);
        uint32_t shares = big_to_native(msg->shares);
        bool isBuy = (msg->buySellIndicator == 'B');
        uint64_t timestamp = msg->header.getTimestamp();

        OrderBook *book = getBookForMsg(ptr);
        if (book != nullptr) {
            book->addOrder(orderId, timestamp, isBuy, price, shares, msg->stock);
        }
    }

    void handleOrderExecuted(const char *ptr) {
        const auto *msg = reinterpret_cast<const OrderExecutedMsg *>(ptr);
        uint64_t orderId = big_to_native(msg->orderReferenceNumber);
        uint32_t shares = big_to_native(msg->executedShares);

        OrderBook *book = getBookForMsg(ptr);
        if (book != nullptr) {
            book->executeOrder(orderId, shares);
        }
    }

    void handleOrderExecutedWithPrice(const char *ptr) {
        const auto *msg = reinterpret_cast<const OrderExecutedWithPriceMsg *>(ptr);
        uint64_t orderId = big_to_native(msg->orderReferenceNumber);
        uint32_t shares = big_to_native(msg->executedShares);
        OrderBook *book = getBookForMsg(ptr);
        if (book != nullptr) {
            book->executeOrder(orderId, shares);
        }
    }

    void handleOrderDelete(const char *ptr) {
        const auto *msg = reinterpret_cast<const OrderDeleteMsg *>(ptr);
        uint64_t orderId = big_to_native(msg->orderReferenceNumber);

        OrderBook *book = getBookForMsg(ptr);
        if (book != nullptr) {
            book->deleteOrder(orderId);
        }
    }

    void handleOrderReplace(const char *ptr) {
        const auto *msg = reinterpret_cast<const OrderReplaceMsg *>(ptr);
        uint64_t oldId = big_to_native(msg->originalOrderReferenceNumber);
        uint64_t newId = big_to_native(msg->newOrderReferenceNumber);
        uint32_t shares = big_to_native(msg->shares);
        uint32_t price = big_to_native(msg->price);

        OrderBook *book = getBookForMsg(ptr);
        if (book != nullptr) {
            book->replaceOrder(oldId, newId, price, shares);
        }
    }

    void handleOrderCancel(const char *ptr) {
        const auto *msg = reinterpret_cast<const OrderCancelMsg *>(ptr);
        uint64_t orderId = big_to_native(msg->orderReferenceNumber);
        uint32_t shares = big_to_native(msg->canceledShares);

        OrderBook *book = getBookForMsg(ptr);
        if (book != nullptr) {
            book->cancelOrder(orderId, shares);
        }
    }

    void handleTrade(const char *ptr) {
        const auto *msg = reinterpret_cast<const TradeMsg *>(ptr);
        if (printDebug && messagesParsed < 10) {
            std::string stock = formatStock(msg->stock);
            double price = formatPrice(msg->price);
            uint32_t shares = big_to_native(msg->shares);
            log("Trade: " + stock + " " + std::to_string(shares) + " @ " + std::to_string(price));
        }
    }

    void handleStockDirectory(const char *ptr) {
        const auto *msg = reinterpret_cast<const StockDirectoryMsg *>(ptr);
        if (printDebug && msgCounts['R'] <= 5) {
            std::string stock = formatStock(msg->stock);
            uint16_t tracking = big_to_native(msg->header.trackingNumber);
            log("Stock Directory: " + stock + ", Tracking=" + std::to_string(tracking));
        }
    }

    static void handleNOII(const char *ptr) {
        const auto *msg = reinterpret_cast<const NOIIMsg *>(ptr);
        (void)msg;
    }

    static void handleCrossTrade(const char *ptr) { (void)ptr; }

    static void handleBrokenTrade(const char *ptr) { (void)ptr; }
};

}  // namespace MarketData
