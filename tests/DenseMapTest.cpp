#include <gtest/gtest.h>

#include <cstring>
#include <iterator>
#include <memory_resource>
#include <string>
#include <unordered_map>
#include <vector>

#include "DenseMap.hpp"
#include "OrderBook.hpp"
#include "OrderBookDefs.hpp"
#include "ShardManager.hpp"

using namespace MarketData;

TEST(DenseMapTest, SequentialInsertFindErase) {
    DenseMap<uint64_t, int> m;
    constexpr size_t n = 10000;
    for (uint64_t i = 1; i <= n; ++i) {
        auto [v, inserted] = m.try_emplace(i, static_cast<int>(i));
        ASSERT_TRUE(inserted);
        ASSERT_EQ(*v, static_cast<int>(i));
    }
    EXPECT_EQ(m.size(), n);

    for (uint64_t i = 1; i <= n; ++i) {
        auto [v, inserted] = m.try_emplace(i, 0);
        EXPECT_FALSE(inserted);
        EXPECT_EQ(*v, static_cast<int>(i));
    }
    EXPECT_EQ(m.size(), n);

    for (uint64_t i = 1; i <= n; i += 3) {
        m.erase(i);
    }
    size_t expect = n - (n + 2) / 3;
    EXPECT_EQ(m.size(), expect);

    for (uint64_t i = 1; i <= n; ++i) {
        int* v = m.find(i);
        if (i % 3 == 1) {
            EXPECT_EQ(v, nullptr);
        } else {
            ASSERT_NE(v, nullptr);
            EXPECT_EQ(*v, static_cast<int>(i));
        }
    }
}

TEST(DenseMapTest, RehashPreservesEntries) {
    DenseMap<uint64_t, int> m;
    EXPECT_GE(m.capacity(), 256u);

    for (uint64_t i = 0; i < 2000; ++i) {
        m.try_emplace(i, static_cast<int>(i * 2));
    }
    EXPECT_GT(m.capacity(), 256u);
    EXPECT_EQ(m.size(), 2000u);

    for (uint64_t i = 0; i < 2000; ++i) {
        int* v = m.find(i);
        ASSERT_NE(v, nullptr);
        EXPECT_EQ(*v, static_cast<int>(i * 2));
    }
}

TEST(DenseMapTest, StringValuesDestroyCleanly) {
    DenseMap<uint64_t, std::string> m;
    m.try_emplace(1, std::string(64, 'a'));
    m.try_emplace(2, std::string(64, 'b'));
    m.erase(1);
    EXPECT_EQ(m.size(), 1u);
    std::string* s = m.find(2);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->size(), 64u);
    m.erase(2);
    EXPECT_EQ(m.size(), 0u);
    EXPECT_EQ(m.find(2), nullptr);
}

TEST(DenseMapTest, OrderValuesMatchBookPath) {
    DenseMap<OrderId, Order> m;
    auto [o, inserted] = m.try_emplace(42);
    ASSERT_TRUE(inserted);
    o->isBuy    = true;
    o->price    = 100;
    o->quantity = 10;
    std::memcpy(o->symbol, "AAPL    ", 8);

    auto [dup, dupInserted] = m.try_emplace(42);
    EXPECT_FALSE(dupInserted);
    EXPECT_EQ(dup->price, 100);
    EXPECT_EQ(m.size(), 1u);

    m.erase(42);
    EXPECT_EQ(m.size(), 0u);
    EXPECT_EQ(m.find(42), nullptr);
}

TEST(DenseMapTest, ForEachVisitsOccupiedOnly) {
    DenseMap<uint64_t, int> m;
    m.try_emplace(10, 1);
    m.try_emplace(20, 2);
    m.try_emplace(30, 3);
    m.erase(20);

    std::vector<uint64_t> keys;
    m.forEach([&](uint64_t k, int) { keys.push_back(k); });
    ASSERT_EQ(keys.size(), 2u);
}

TEST(DenseMapTest, MonotonicArenaRehashAndErase) {
    std::pmr::monotonic_buffer_resource arena(1024);
    DenseMap<uint64_t, Order> m(&arena);
    constexpr uint64_t n = 5000;
    for (uint64_t i = 1; i <= n; ++i) {
        auto [o, inserted] = m.try_emplace(i);
        ASSERT_TRUE(inserted);
        o->quantity = static_cast<Quantity>(i);
    }
    EXPECT_EQ(m.size(), n);
    for (uint64_t i = 1; i <= n; ++i) {
        Order* o = m.find(i);
        ASSERT_NE(o, nullptr);
        EXPECT_EQ(o->quantity, static_cast<Quantity>(i));
    }
    for (uint64_t i = 1; i <= n; i += 2) {
        m.erase(i);
    }
    EXPECT_EQ(m.size(), n / 2);
    for (uint64_t i = 1; i <= n; ++i) {
        Order* o = m.find(i);
        if (i % 2 == 1) {
            EXPECT_EQ(o, nullptr);
        } else {
            ASSERT_NE(o, nullptr);
        }
    }
}

TEST(DenseMapTest, MatchesUnorderedMapChurn) {
    DenseMap<uint64_t, int> m;
    std::unordered_map<uint64_t, int> ref;
    uint64_t id = 0x0000000100000000ULL;
    for (int i = 0; i < 20000; ++i) {
        id += 17;
        if ((i & 3) == 3 && !ref.empty()) {
            auto it = ref.begin();
            std::advance(it, static_cast<long>(i % static_cast<int>(ref.size())));
            uint64_t k = it->first;
            m.erase(k);
            ref.erase(k);
        } else {
            int val = i;
            auto [p, inserted] = m.try_emplace(id, val);
            auto [it, refIns] = ref.emplace(id, val);
            EXPECT_EQ(inserted, refIns);
            if (!inserted) {
                EXPECT_EQ(*p, it->second);
            }
        }
    }
    EXPECT_EQ(m.size(), ref.size());
    for (const auto& [k, v] : ref) {
        int* p = m.find(k);
        ASSERT_NE(p, nullptr);
        EXPECT_EQ(*p, v);
    }
}

TEST(DenseMapTest, ShardManagerLiveCount) {
    ShardManager sm(32 * 1024 * 1024, 512);
    for (int i = 0; i < 4000; ++i) {
        sm.getBook(static_cast<size_t>(i % 300))->addOrder(
            static_cast<OrderId>(i + 1), true, 100, 10, "AAPL    ");
    }
    EXPECT_EQ(sm.getTotalOrderCount(), 4000u);
    for (int i = 0; i < 4000; ++i) {
        sm.getBook(static_cast<size_t>(i % 300))->deleteOrder(static_cast<OrderId>(i + 1));
    }
    EXPECT_EQ(sm.getTotalOrderCount(), 0u);
    for (int i = 0; i < 46056; ++i) {
        sm.getBook(static_cast<size_t>(i % 400))->addOrder(
            static_cast<OrderId>(1000000 + i), false, 1, 1, "MSFT    ");
    }
    EXPECT_EQ(sm.getTotalOrderCount(), 46056u);
}
