#include <gtest/gtest.h>
#include <chrono>
#include <string>
#include <thread>
#include "../../../src/application/helper/memory_cache.h"

namespace application
{
    namespace helper
    {
        TEST(MemoryCacheTest, TryGetMissingKey)
        {
            const std::chrono::seconds cLifetime{60};
            const std::string cUntouchedValue{"untouched"};

            MemoryCache<int, std::string> _cache(cLifetime);
            std::string _item{cUntouchedValue};

            EXPECT_FALSE(_cache.TryGet(1, _item));
            EXPECT_EQ(_item, cUntouchedValue);
        }

        TEST(MemoryCacheTest, AddAndTryGet)
        {
            const std::chrono::seconds cLifetime{60};
            const int cKey{1};
            const std::string cValue{"cached"};

            MemoryCache<int, std::string> _cache(cLifetime);
            _cache.Add(cKey, cValue);

            std::string _item;
            EXPECT_TRUE(_cache.TryGet(cKey, _item));
            EXPECT_EQ(_item, cValue);

            // Retrieval does not remove a valid item
            std::string _secondItem;
            EXPECT_TRUE(_cache.TryGet(cKey, _secondItem));
            EXPECT_EQ(_secondItem, cValue);
        }

        TEST(MemoryCacheTest, MultipleKeys)
        {
            const std::chrono::seconds cLifetime{60};
            const std::string cValue1{"first"};
            const std::string cValue2{"second"};

            MemoryCache<std::string, std::string> _cache(cLifetime);
            _cache.Add("key1", cValue1);
            _cache.Add("key2", cValue2);

            std::string _item;
            EXPECT_TRUE(_cache.TryGet("key1", _item));
            EXPECT_EQ(_item, cValue1);
            EXPECT_TRUE(_cache.TryGet("key2", _item));
            EXPECT_EQ(_item, cValue2);
            EXPECT_FALSE(_cache.TryGet("key3", _item));
            EXPECT_EQ(_item, cValue2);
        }

        TEST(MemoryCacheTest, DuplicateAddKeepsFirstItem)
        {
            const std::chrono::seconds cLifetime{60};
            const int cKey{1};
            const int cFirstValue{10};
            const int cSecondValue{20};

            MemoryCache<int, int> _cache(cLifetime);
            _cache.Add(cKey, cFirstValue);
            _cache.Add(cKey, cSecondValue);

            int _item{0};
            EXPECT_TRUE(_cache.TryGet(cKey, _item));
            EXPECT_EQ(_item, cFirstValue);
        }

        TEST(MemoryCacheTest, ZeroLifetimeExpiresImmediately)
        {
            const std::chrono::seconds cLifetime{0};
            const int cKey{1};
            const int cValue{10};
            const int cUntouchedValue{-1};

            MemoryCache<int, int> _cache(cLifetime);
            _cache.Add(cKey, cValue);

            int _item{cUntouchedValue};
            EXPECT_FALSE(_cache.TryGet(cKey, _item));
            EXPECT_EQ(_item, cUntouchedValue);
        }

        TEST(MemoryCacheTest, ExpiredItemEviction)
        {
            const std::chrono::seconds cLifetime{1};
            const std::chrono::milliseconds cWaitTime{1100};
            const int cKey{1};
            const int cExpiredValue{10};
            const int cFreshValue{20};

            MemoryCache<int, int> _cache(cLifetime);
            _cache.Add(cKey, cExpiredValue);

            int _item{0};
            EXPECT_TRUE(_cache.TryGet(cKey, _item));
            EXPECT_EQ(_item, cExpiredValue);

            std::this_thread::sleep_for(cWaitTime);

            int _expiredItem{0};
            EXPECT_FALSE(_cache.TryGet(cKey, _expiredItem));
            EXPECT_EQ(_expiredItem, 0);

            // Re-adding succeeds only if the expired item has been evicted
            _cache.Add(cKey, cFreshValue);
            EXPECT_TRUE(_cache.TryGet(cKey, _item));
            EXPECT_EQ(_item, cFreshValue);
        }
    }
}
