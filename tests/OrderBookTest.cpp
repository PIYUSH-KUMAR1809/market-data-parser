#include <gtest/gtest.h>

#include "OrderBook.hpp"

using namespace MarketData;

class OrderBookTest : public ::testing::Test {
   protected:
    OrderBook book;
};

TEST_F(OrderBookTest, AddOrderIncreasesCount) {
    EXPECT_EQ(book.getOrderCount(), 0);

    book.addOrder(1, 1000, true, 100, 10, "AAPL1234");
    EXPECT_EQ(book.getOrderCount(), 1);
}

TEST_F(OrderBookTest, ExecuteOrderReducesQuantity) {
    book.addOrder(1, 1000, true, 100, 10, "AAPL1234");
    book.executeOrder(1, 5);

    EXPECT_EQ(book.getOrderCount(), 1);
    book.executeOrder(1, 5);
    EXPECT_EQ(book.getOrderCount(), 0);
}

TEST_F(OrderBookTest, CancelOrderRemovesOrder) {
    book.addOrder(1, 1000, true, 100, 10, "AAPL1234");
    book.cancelOrder(1, 10);
    EXPECT_EQ(book.getOrderCount(), 0);
}

TEST_F(OrderBookTest, DuplicatesIgnored) {
    book.addOrder(1, 1000, true, 100, 10, "AAPL1234");
    book.addOrder(1, 1000, true, 200, 20, "AAPL1234");
    EXPECT_EQ(book.getOrderCount(), 1);
}
