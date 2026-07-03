#pragma once

#include <functional>
#include <map>
#include <memory_resource>
#include <string>
#include <vector>

#include "DenseMap.hpp"
#include "OrderBookDefs.hpp"

namespace MarketData {

template <typename T>
using PmrVector = std::vector<T, std::pmr::polymorphic_allocator<T>>;

template <typename Key, typename Value, typename Compare = std::less<Key>>
using PmrMap =
    std::map<Key, Value, Compare, std::pmr::polymorphic_allocator<std::pair<const Key, Value>>>;

class OrderBook {
   public:
    using BboCallback = std::function<void(const std::string& symbol,
                                           uint32_t bidPrice,
                                           uint32_t bidQty,
                                           uint32_t askPrice,
                                           uint32_t askQty)>;

    explicit OrderBook(std::pmr::memory_resource* mr = std::pmr::get_default_resource())
        : orderMap_(mr), bids_(mr), asks_(mr) {}

    void setBboCallback(BboCallback cb) { bboCallback_ = std::move(cb); }

    void addOrder(OrderId id, // NOLINT(bugprone-easily-swappable-parameters)
                  uint64_t timestamp,
                  bool isBuy,
                  Price price, // NOLINT(bugprone-easily-swappable-parameters)
                  Quantity quantity,
                  const char* symbol) {
        Order& order = orderMap_.emplace(id);

        if (order.id == id && order.quantity > 0) {
            return;
        }

        order.id = id;
        order.timestamp = timestamp;
        order.isBuy = isBuy;
        order.price = price;
        order.quantity = quantity;
        std::copy(symbol, symbol + 8, order.symbol);

        if (isBuy) {
            PriceLevel& level = bids_[price];
            level.price = price;
            level.totalQuantity += quantity;
            level.orderCount++;
        } else {
            PriceLevel& level = asks_[price];
            level.price = price;
            level.totalQuantity += quantity;
            level.orderCount++;
        }
    }

    void executeOrder(OrderId id, // NOLINT(bugprone-easily-swappable-parameters)
                      Quantity executedQty) {
        Order* order = orderMap_.find(id);
        if (order == nullptr) return;

        if (executedQty > order->quantity) {
            executedQty = order->quantity;
        }

        reduceLevel(order->isBuy, order->price, executedQty);

        order->quantity -= executedQty;
        if (order->quantity == 0) {
            orderMap_.erase(id);
        }
    }

    void cancelOrder(OrderId id, Quantity canceledQty) { executeOrder(id, canceledQty); }

    void deleteOrder(OrderId id) {
        Order* order = orderMap_.find(id);
        if (order == nullptr) return;

        reduceLevel(order->isBuy, order->price, order->quantity);
        orderMap_.erase(id);
    }

    void replaceOrder(OrderId oldId, // NOLINT(bugprone-easily-swappable-parameters)
                      OrderId newId,
                      Price newPrice, // NOLINT(bugprone-easily-swappable-parameters)
                      Quantity newQty) {
        Order* oldOrderPtr = orderMap_.find(oldId);
        if (oldOrderPtr == nullptr) return;

        Order oldOrder = *oldOrderPtr;

        deleteOrder(oldId);
        addOrder(newId, oldOrder.timestamp, oldOrder.isBuy, newPrice, newQty, oldOrder.symbol);
    }

    size_t getOrderCount() const { return orderMap_.size(); }

   private:
    DenseMap<OrderId, Order> orderMap_;
    BboCallback bboCallback_;

    PmrMap<Price, PriceLevel, std::greater<Price>> bids_;
    PmrMap<Price, PriceLevel> asks_;

    void reduceLevel(bool isBuy,
                     Price price, // NOLINT(bugprone-easily-swappable-parameters)
                     Quantity qty) {
        if (isBuy) {
            auto it = bids_.find(price);
            if (it != bids_.end()) {
                it->second.totalQuantity -= qty;
                if (it->second.totalQuantity == 0) {
                    bids_.erase(it);
                }
            }
        } else {
            auto it = asks_.find(price);
            if (it != asks_.end()) {
                it->second.totalQuantity -= qty;
                if (it->second.totalQuantity == 0) {
                    asks_.erase(it);
                }
            }
        }
    }
};

}
