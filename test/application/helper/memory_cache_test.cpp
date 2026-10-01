#include <gtest/gtest.h>
#include <string>
#include "../../../src/application/helper/memory_cache.h"

namespace application
{
    namespace helper
    {
        TEST(MemoryCacheTest, TryGetMissingKey)
        {
            const std::chrono::seconds cLifetime{60};
            const int cKey{1};

            MemoryCache<int, std::string> _cache(cLifetime);
            std::string _item;

            EXPECT_FALSE(_cache.TryGet(cKey, _item));
            EXPECT_TRUE(_item.empty());
        }

        TEST(MemoryCacheTest, AddAndTryGet)
        {
            const std::chrono::seconds cLifetime{60};
            const int cKey{1};
            const std::string cItem{"cached value"};

            MemoryCache<int, std::string> _cache(cLifetime);
            _cache.Add(cKey, cItem);

            std::string _item;
            EXPECT_TRUE(_cache.TryGet(cKey, _item));
            EXPECT_EQ(cItem, _item);

            // A cache hit must not evict the item.
            _item.clear();
            EXPECT_TRUE(_cache.TryGet(cKey, _item));
            EXPECT_EQ(cItem, _item);
        }

        TEST(MemoryCacheTest, MultipleKeys)
        {
            const std::chrono::seconds cLifetime{60};
            const uint16_t cFirstKey{0xf40d};
            const uint16_t cSecondKey{0xf42f};
            const int cFirstItem{10};
            const int cSecondItem{20};

            MemoryCache<uint16_t, int> _cache(cLifetime);
            _cache.Add(cFirstKey, cFirstItem);
            _cache.Add(cSecondKey, cSecondItem);

            int _item;
            EXPECT_TRUE(_cache.TryGet(cFirstKey, _item));
            EXPECT_EQ(cFirstItem, _item);
            EXPECT_TRUE(_cache.TryGet(cSecondKey, _item));
            EXPECT_EQ(cSecondItem, _item);
        }

        TEST(MemoryCacheTest, DuplicateKeyKeepsOriginal)
        {
            const std::chrono::seconds cLifetime{60};
            const int cKey{1};
            const std::string cOriginalItem{"original"};
            const std::string cNewItem{"new"};

            MemoryCache<int, std::string> _cache(cLifetime);
            _cache.Add(cKey, cOriginalItem);
            _cache.Add(cKey, cNewItem);

            std::string _item;
            EXPECT_TRUE(_cache.TryGet(cKey, _item));
            EXPECT_EQ(cOriginalItem, _item);
        }

        TEST(MemoryCacheTest, ExpiredItem)
        {
            const std::chrono::seconds cZeroLifetime{0};
            const int cKey{1};
            const std::string cItem{"expired"};

            MemoryCache<int, std::string> _cache(cZeroLifetime);
            _cache.Add(cKey, cItem);

            std::string _item;
            EXPECT_FALSE(_cache.TryGet(cKey, _item));
            EXPECT_TRUE(_item.empty());
        }
    }
}
