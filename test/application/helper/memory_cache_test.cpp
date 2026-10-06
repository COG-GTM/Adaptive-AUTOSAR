#include <gtest/gtest.h>
#include <string>
#include "../../../src/application/helper/memory_cache.h"

namespace application
{
    namespace helper
    {
        TEST(MemoryCacheTest, MissingItem)
        {
            MemoryCache<int, std::string> _cache(std::chrono::seconds(60));
            std::string _item{"unchanged"};

            EXPECT_FALSE(_cache.TryGet(1, _item));
            EXPECT_EQ("unchanged", _item);
        }

        TEST(MemoryCacheTest, AddAndGet)
        {
            MemoryCache<int, std::string> _cache(std::chrono::seconds(60));
            _cache.Add(1, "first");
            _cache.Add(2, "second");

            std::string _item;
            EXPECT_TRUE(_cache.TryGet(1, _item));
            EXPECT_EQ("first", _item);
            EXPECT_TRUE(_cache.TryGet(2, _item));
            EXPECT_EQ("second", _item);
            EXPECT_TRUE(_cache.TryGet(1, _item));
            EXPECT_EQ("first", _item);
        }

        TEST(MemoryCacheTest, AddDoesNotOverwriteLiveItem)
        {
            MemoryCache<int, std::string> _cache(std::chrono::seconds(60));
            _cache.Add(1, "first");
            _cache.Add(1, "second");

            std::string _item;
            EXPECT_TRUE(_cache.TryGet(1, _item));
            EXPECT_EQ("first", _item);
        }

        TEST(MemoryCacheTest, ExpiredItemIsEvicted)
        {
            MemoryCache<int, std::string> _cache(std::chrono::seconds(0));
            _cache.Add(1, "first");

            std::string _item{"unchanged"};
            EXPECT_FALSE(_cache.TryGet(1, _item));
            EXPECT_EQ("unchanged", _item);

            // After eviction, the key can be populated again
            _cache.Add(1, "second");
            EXPECT_FALSE(_cache.TryGet(1, _item));
        }
    }
}
