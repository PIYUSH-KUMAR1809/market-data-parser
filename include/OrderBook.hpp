#pragma once

#include <cstring>
#include <memory_resource>

#include "DenseMap.hpp"
#include "OrderBookDefs.hpp"

namespace MarketData {

class OrderBook {
   public:
    explicit OrderBook(std::pmr::memory_resource* mr = std::pmr::get_default_resource())
        : orderMap_(mr) {}

    void reserveOrders(size_t n) { orderMap_.reserve(n); }

    void addOrder(OrderId id, bool isBuy, Price price, Quantity quantity, const char* symbol) {
        const size_t before = orderMap_.size();
        Order& order = orderMap_.emplace(id);
        if (orderMap_.size() == before) {
            return;
        }
        order.isBuy    = isBuy;
        order.price    = price;
        order.quantity = quantity;
        std::memcpy(order.symbol, symbol, 8);
    }

    void executeOrder(OrderId id, Quantity executedQty) {
        Order* order = orderMap_.find(id);
        if (order == nullptr) return;
        if (executedQty >= order->quantity) {
            orderMap_.erase(id);
            return;
        }
        order->quantity -= executedQty;
    }

    void cancelOrder(OrderId id, Quantity canceledQty) { executeOrder(id, canceledQty); }

    void deleteOrder(OrderId id) { orderMap_.erase(id); }

    void replaceOrder(OrderId oldId, OrderId newId, Price newPrice, Quantity newQty) {
        Order* oldOrderPtr = orderMap_.find(oldId);
        if (oldOrderPtr == nullptr) return;
        Order oldOrder = *oldOrderPtr;
        orderMap_.erase(oldId);
        addOrder(newId, oldOrder.isBuy, newPrice, newQty, oldOrder.symbol);
    }

    size_t getOrderCount() const { return orderMap_.size(); }

    void mergeFrom(const OrderBook& other) {
        other.orderMap_.forEach([this](OrderId id, const Order& order) {
            this->addOrder(id, order.isBuy, order.price, order.quantity, order.symbol);
        });
    }

   private:
    DenseMap<OrderId, Order> orderMap_;
};

}
