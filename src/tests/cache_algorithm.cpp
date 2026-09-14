#include <gtest/gtest.h>

#include <array>
#include <algorithm>
#include <random>
#include <string>
#include <vector>

#include <cache_algorithm/arc.hpp>
#include <cache_algorithm/lfu_cache.hpp>
#include <cache_algorithm/lirs_cache.hpp>
#include <cache_algorithm/lru_cache.hpp>
#include <cache_algorithm/two_queues.hpp>

namespace {
int LoadPage(const int& key) {
  return key * 10;
}

// A fresh load returns its call number; a hit must keep the stored version.
struct ExpectedAccess {
  int key;
  int value;
  size_t misses;
};
} // namespace

// ============================================================================
// === LFU cache ===
// ============================================================================
TEST(LfuCacheTest, ConstructorInitializesCountersToZero) {
  int loader_calls = 0;
  cache::Lfu<int, int> cache(2, [&](const int& key) {
    ++loader_calls;
    return LoadPage(key);
  });

  EXPECT_EQ(loader_calls, 0);
  EXPECT_EQ(cache.GetCacheMissCount(), 0);
  EXPECT_EQ(cache.GetAccessCount(), 0);
}

TEST(LfuCacheTest, FirstLookupLoadsPage) {
  int loader_calls = 0;
  cache::Lfu<int, int> cache(2, [&](const int& key) {
    ++loader_calls;
    EXPECT_EQ(key, 7);
    return LoadPage(key);
  });

  EXPECT_EQ(cache.LookUpUpdate(7), 70);
  EXPECT_EQ(loader_calls, 1);
  EXPECT_EQ(cache.GetCacheMissCount(), 1);
  EXPECT_EQ(cache.GetAccessCount(), 1);
}

TEST(LfuCacheTest, RepeatedLookupDoesNotCallLoader) {
  int loader_calls = 0;
  cache::Lfu<int, int> cache(2, [&](const int& key) {
    return LoadPage(key) * ++loader_calls;
  });

  EXPECT_EQ(cache.LookUpUpdate(7), 70);
  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(cache.LookUpUpdate(7), 70);
  }
  EXPECT_EQ(loader_calls, 1);
  EXPECT_EQ(cache.GetCacheMissCount(), 1);
  EXPECT_EQ(cache.GetAccessCount(), 101);
}

TEST(LfuCacheTest, InstancesHaveIndependentState) {
  cache::Lfu<int, int> first(2, LoadPage);
  cache::Lfu<int, int> second(2, [](const int& key) { return key * 100; });

  EXPECT_EQ(first.LookUpUpdate(7), 70);
  EXPECT_EQ(second.GetAccessCount(), 0);
  EXPECT_EQ(second.GetCacheMissCount(), 0);
  EXPECT_EQ(second.LookUpUpdate(7), 700);
  EXPECT_EQ(first.LookUpUpdate(7), 70);
  EXPECT_EQ(first.GetCacheMissCount(), 1);
  EXPECT_EQ(first.GetAccessCount(), 2);
  EXPECT_EQ(second.GetCacheMissCount(), 1);
  EXPECT_EQ(second.GetAccessCount(), 1);
}

TEST(LfuCacheTest, EvictsLeastFrequentEvenWhenItWasUsedMostRecently) {
  int loader_calls = 0;
  cache::Lfu<int, int> cache(2, [&](const int& key) {
    ++loader_calls;
    return LoadPage(key);
  });
  for (int key : {1, 1, 1, 2, 2, 3}) {
    cache.LookUpUpdate(key);
  }
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.LookUpUpdate(3), 30);
  EXPECT_EQ(cache.GetCacheMissCount(), 3);
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.GetCacheMissCount(), 4);
  EXPECT_EQ(cache.GetAccessCount(), 9);
  EXPECT_EQ(loader_calls, 4);
}

TEST(LfuCacheTest, EqualInitialFrequenciesEvictOldestPage) {
  int loader_calls = 0;
  cache::Lfu<int, int> cache(2, [&](const int& key) {
    ++loader_calls;
    return LoadPage(key);
  });
  for (int key : {1, 2, 3}) {
    cache.LookUpUpdate(key);
  }
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.LookUpUpdate(3), 30);
  EXPECT_EQ(cache.GetCacheMissCount(), 3);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 4);
  EXPECT_EQ(loader_calls, 4);
}

TEST(LfuCacheTest, EqualPromotedFrequenciesEvictLeastRecentlyUsedPage) {
  int loader_calls = 0;
  cache::Lfu<int, int> cache(2, [&](const int& key) {
    ++loader_calls;
    return LoadPage(key);
  });
  // Both pages reach frequency 2, but page 2 reaches it first.
  for (int key : {1, 2, 2, 1, 3}) {
    cache.LookUpUpdate(key);
  }
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.LookUpUpdate(3), 30);
  EXPECT_EQ(cache.GetCacheMissCount(), 3);
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.GetCacheMissCount(), 4);
  EXPECT_EQ(loader_calls, 4);
}

TEST(LfuCacheTest, CapacityOneReloadsEvictedPageWithFreshValue) {
  int version = 0;
  auto loader = [&](const int&) { return ++version; };
  cache::Lfu<int, int> cache(1, loader);
  EXPECT_EQ(cache.LookUpUpdate(1), 1);
  EXPECT_EQ(cache.LookUpUpdate(1), 1);
  EXPECT_EQ(cache.LookUpUpdate(2), 2);
  EXPECT_EQ(cache.LookUpUpdate(1), 3);
  EXPECT_EQ(cache.LookUpUpdate(1), 3);
  EXPECT_EQ(version, 3);
  EXPECT_EQ(cache.GetCacheMissCount(), 3);
  EXPECT_EQ(cache.GetAccessCount(), 5);
}

TEST(LfuCacheTest, ReloadedPageStartsAtMinimumFrequency) {
  int loader_calls = 0;
  cache::Lfu<int, int> cache(2, [&](const int& key) {
    ++loader_calls;
    return LoadPage(key);
  });
  for (int key : {1, 1, 2, 2, 2, 3, 1, 4}) {
    cache.LookUpUpdate(key);
  }
  // Reloaded 1 has frequency 1 and is evicted by 4; 2 stays at 3.
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.LookUpUpdate(4), 40);
  EXPECT_EQ(cache.GetCacheMissCount(), 5);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 6);
  EXPECT_EQ(loader_calls, 6);
}

TEST(LfuCacheTest, WorkingSetFitsWithoutFurtherLoads) {
  for (size_t capacity : {1, 2, 3, 16}) {
    SCOPED_TRACE(capacity);
    size_t loader_calls = 0;
    auto loader = [&](const int& key) {
      ++loader_calls;
      return LoadPage(key);
    };
    cache::Lfu<int, int> cache(capacity, loader);
    for (int round = 0; round < 10; ++round) {
      for (size_t key = 0; key < capacity; ++key) {
        EXPECT_EQ(cache.LookUpUpdate(static_cast<int>(key)),
                  LoadPage(static_cast<int>(key)));
      }
    }
    EXPECT_EQ(loader_calls, capacity);
    EXPECT_EQ(cache.GetCacheMissCount(), capacity);
    EXPECT_EQ(cache.GetAccessCount(), 10 * capacity);
  }
}

TEST(LfuCacheTest, SupportsStringKeysAndValues) {
  auto loader = [](const std::string& key) { return "value:" + key; };
  cache::Lfu<std::string, std::string> cache(2, loader);
  EXPECT_EQ(cache.LookUpUpdate(""), "value:");
  EXPECT_EQ(cache.LookUpUpdate("hello"), "value:hello");
  EXPECT_EQ(cache.LookUpUpdate(""), "value:");
  EXPECT_EQ(cache.LookUpUpdate("world"), "value:world");
  EXPECT_EQ(cache.LookUpUpdate(""), "value:");
  EXPECT_EQ(cache.GetCacheMissCount(), 3);
  EXPECT_EQ(cache.LookUpUpdate("hello"), "value:hello");
  EXPECT_EQ(cache.GetCacheMissCount(), 4);
}

TEST(LfuCacheTest, MatchesReferenceModelAcrossChangingWorkingSets) {
  // A linear reference model independent of the LFU frequency-list structure.
  struct Entry {
    int key;
    int value;
    size_t frequency;
    size_t last_access;
  };
  for (size_t capacity : {1, 2, 3, 8, 16}) {
    SCOPED_TRACE(capacity);
    std::vector<Entry> model;
    std::mt19937 random(42);
    int expected_loads = 0;
    int actual_loads = 0;
    auto loader = [&](const int&) { return ++actual_loads; };
    cache::Lfu<int, int> cache(capacity, loader);
    for (size_t step = 0; step < 2000; ++step) {
      const int key = static_cast<int>(random() % (step % 100 < 50 ? 5 : 23));
      SCOPED_TRACE(testing::Message() << "step=" << step << ", key=" << key);
      auto found = std::find_if(model.begin(), model.end(),
                                [&](const Entry& entry) { return entry.key == key; });
      int expected_value;
      if (found != model.end()) {
        ++found->frequency;
        found->last_access = step;
        expected_value = found->value;
      } else {
        if (model.size() == capacity) {
          auto victim = std::min_element(model.begin(), model.end(),
              [](const Entry& a, const Entry& b) {
                return a.frequency < b.frequency ||
                       (a.frequency == b.frequency && a.last_access < b.last_access);
              });
          model.erase(victim);
        }
        expected_value = ++expected_loads;
        model.push_back({key, expected_value, 1, step});
      }
      ASSERT_EQ(cache.LookUpUpdate(key), expected_value);
      ASSERT_EQ(actual_loads, expected_loads);
      ASSERT_EQ(cache.GetCacheMissCount(), static_cast<size_t>(expected_loads));
      ASSERT_EQ(cache.GetAccessCount(), step + 1);
    }
  }
}

// Expected versions and misses were calculated with a separate simulation.
TEST(LfuCacheTest, HundredTwentyAccessesWithFrequencyTiesAndWorkingSetChanges) {
  // Each row is {key, loaded value version, cumulative misses}.
  const std::array<ExpectedAccess, 120> accesses = {{
      // Raise all four frequencies equally, then break ties by last access.
      {1, 1, 1}, {2, 2, 2}, {3, 3, 3}, {4, 4, 4},
      {2, 2, 4}, {1, 1, 4}, {4, 4, 4}, {3, 3, 4},
      {1, 1, 4}, {2, 2, 4}, {3, 3, 4}, {4, 4, 4},
      {5, 5, 5}, {2, 2, 5}, {3, 3, 5}, {4, 4, 5},
      {5, 5, 5}, {5, 5, 5}, {5, 5, 5}, {6, 6, 6},
      // Build new frequency groups while old groups lose their final pages.
      {2, 7, 7}, {3, 3, 7}, {4, 4, 7}, {6, 8, 8},
      {6, 8, 8}, {6, 8, 8}, {6, 8, 8}, {7, 9, 9},
      {3, 3, 9}, {4, 4, 9}, {7, 9, 9}, {7, 9, 9},
      {7, 9, 9}, {7, 9, 9}, {8, 10, 10}, {4, 4, 10},
      {8, 10, 10}, {8, 10, 10}, {8, 10, 10}, {8, 10, 10},
      // Switch to pages 9 through 12, then make page 9 frequent.
      {9, 11, 11}, {9, 11, 11}, {10, 12, 12}, {10, 12, 12},
      {9, 13, 13}, {10, 14, 14}, {11, 15, 15}, {11, 15, 15},
      {12, 16, 16}, {12, 16, 16}, {9, 17, 17}, {10, 18, 18},
      {11, 19, 19}, {12, 20, 20}, {9, 21, 21}, {9, 21, 21},
      {9, 21, 21}, {9, 21, 21}, {9, 21, 21}, {9, 21, 21},
      // Return to the original keys and rebuild their frequencies after eviction.
      {1, 22, 22}, {2, 23, 23}, {3, 3, 23}, {4, 4, 23},
      {1, 24, 24}, {1, 24, 24}, {1, 24, 24}, {1, 24, 24},
      {1, 24, 24}, {1, 24, 24}, {2, 25, 25}, {2, 25, 25},
      {2, 25, 25}, {2, 25, 25}, {2, 25, 25}, {2, 25, 25},
      {3, 3, 25}, {3, 3, 25}, {3, 3, 25}, {3, 3, 25},
      // Scan ten new keys, then revisit the surviving frequent pages.
      {20, 26, 26}, {21, 27, 27}, {22, 28, 28}, {23, 29, 29},
      {24, 30, 30}, {25, 31, 31}, {26, 32, 32}, {27, 33, 33},
      {28, 34, 34}, {29, 35, 35}, {1, 36, 36}, {2, 25, 36},
      {3, 3, 36}, {4, 4, 36}, {1, 36, 36}, {2, 25, 36},
      {3, 3, 36}, {4, 4, 36}, {1, 36, 36}, {2, 25, 36},
      // Raise page 4 repeatedly and interleave old and newly loaded pages.
      {4, 4, 36}, {4, 4, 36}, {4, 4, 36}, {4, 4, 36},
      {4, 4, 36}, {4, 4, 36}, {4, 4, 36}, {4, 4, 36},
      {5, 37, 37}, {5, 37, 37}, {5, 37, 37}, {5, 37, 37},
      {6, 38, 38}, {6, 38, 38}, {6, 38, 38}, {6, 38, 38},
      {4, 4, 38}, {1, 39, 39}, {2, 25, 39}, {3, 3, 39},
  }};
  int loader_calls = 0;
  auto loader = [&](const int&) { return ++loader_calls; };
  cache::Lfu<int, int> cache(4, loader);

  for (size_t i = 0; i < accesses.size(); ++i) {
    const auto& access = accesses[i];
    SCOPED_TRACE(testing::Message() << "access=" << i + 1
                                  << ", key=" << access.key);
    EXPECT_EQ(cache.LookUpUpdate(access.key), access.value);
    EXPECT_EQ(cache.GetCacheMissCount(), access.misses);
    EXPECT_EQ(loader_calls, access.misses);
    EXPECT_EQ(cache.GetAccessCount(), i + 1);
  }
}

TEST(LfuCacheTest, SixtyMixedAccessesAcrossCapacities) {
  const std::array<int, 60> keys = {
      // Reuse zero and negative keys while filling the cache.
      0, -1, 0, 1, 2, -1, 0, 3, 1, 2,
      // Interleave new pages with recently evicted pages.
      4, 5, 3, 4, 6, 5, 7, 6, 8, 7,
      // Return to a small working set and hit it repeatedly.
      0, -1, 0, -1, 1, 2, 1, 2, 3, 3,
      // Scan ten distinct pages to overflow small caches and their histories.
      10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
      // Reverse the end of the scan, then return to the original working set.
      19, 18, 17, 16, 0, -1, 0, -1, 1, 2,
      // Alternate two small sets before revisiting the oldest keys.
      3, 4, 3, 4, 5, 6, 5, 6, 0, -1,
  };
  const std::array<size_t, 6> capacities = {1, 2, 3, 4, 8, 20};
  // Independently simulated outcomes: M = load, H = resident hit.
  // Each string corresponds to the capacity at the same index above.
  const std::array<std::string, 6> expected_outcomes = {
      "MMMMMMMMMM" "MMMMMMMMMM" "MMMMMMMMMH" "MMMMMMMMMM" "HMMMMMMMMM" "MMMMMMMMMM",
      "MMHMMMHMMM" "MMMMMMMMMM" "HMHHMMMMMH" "MMMMMMMMMM" "HMMMHMHHMM" "MMMMMMMMHM",
      "MMHMMMHMMM" "MMMMMMMMMM" "HMHHMMMMMH" "MMMMMMMMMM" "HMMMHMHHMM" "MMMMMMMMHH",
      "MMHMMHHMMM" "MMMMMMMMMM" "HHHHMMHHMH" "MMMMMMMMMM" "HMMMHHHHMM" "MMMMMMMMHH",
      "MMHMMHHMHH" "MMHHMHMMMM" "HHHHHHHHHH" "MMMMMMMMMM" "HMMMHHHHHH" "HMHHHMHHHH",
      "MMHMMHHMHH" "MMHHMHMHMH" "HHHHHHHHHH" "MMMMMMMMMM" "HHHHHHHHHH" "HHHHHHHHHH",
  };

  for (size_t scenario = 0; scenario < capacities.size(); ++scenario) {
    SCOPED_TRACE(testing::Message() << "capacity=" << capacities[scenario]);
    ASSERT_EQ(expected_outcomes[scenario].size(), keys.size());
    size_t loader_calls = 0;
    size_t expected_misses = 0;
    auto loader = [&](const int& key) {
      ++loader_calls;
      return LoadPage(key);
    };
    cache::Lfu<int, int> cache(capacities[scenario], loader);

    for (size_t i = 0; i < keys.size(); ++i) {
      SCOPED_TRACE(testing::Message() << "access=" << i + 1
                                    << ", key=" << keys[i]);
      if (expected_outcomes[scenario][i] == 'M') {
        ++expected_misses;
      }
      EXPECT_EQ(cache.LookUpUpdate(keys[i]), LoadPage(keys[i]));
      EXPECT_EQ(cache.GetCacheMissCount(), expected_misses);
      EXPECT_EQ(loader_calls, expected_misses);
      EXPECT_EQ(cache.GetAccessCount(), i + 1);
    }
  }
}

// ============================================================================
// === LRU cache ===
// ============================================================================
// Evicts the least recently used page when the cache is full.
TEST(LruCacheTest, ConstructorInitializesCountersToZero) {
  int loader_calls = 0;
  cache::Lru<int, int> cache(2, [&](const int& key) {
    ++loader_calls;
    return LoadPage(key);
  });

  EXPECT_EQ(loader_calls, 0);
  EXPECT_EQ(cache.GetCacheMissCount(), 0);
  EXPECT_EQ(cache.GetAccessCount(), 0);
}

TEST(LruCacheTest, FirstLookupLoadsPage) {
  int loader_calls = 0;
  cache::Lru<int, int> cache(2, [&](const int& key) {
    ++loader_calls;
    EXPECT_EQ(key, 7);
    return LoadPage(key);
  });

  EXPECT_EQ(cache.LookUpUpdate(7), 70);
  EXPECT_EQ(loader_calls, 1);
  EXPECT_EQ(cache.GetCacheMissCount(), 1);
  EXPECT_EQ(cache.GetAccessCount(), 1);
}

TEST(LruCacheTest, RepeatedLookupUsesCachedPage) {
  int loader_calls = 0;
  cache::Lru<int, int> cache(2, [&](const int& key) {
    return LoadPage(key) * ++loader_calls;
  });

  EXPECT_EQ(cache.LookUpUpdate(7), 70);
  EXPECT_EQ(cache.LookUpUpdate(7), 70);
  EXPECT_EQ(loader_calls, 1);
  EXPECT_EQ(cache.GetCacheMissCount(), 1);
  EXPECT_EQ(cache.GetAccessCount(), 2);
}

TEST(LruCacheTest, InstancesHaveIndependentState) {
  cache::Lru<int, int> first(2, LoadPage);
  cache::Lru<int, int> second(2, [](const int& key) { return key * 100; });

  EXPECT_EQ(first.LookUpUpdate(7), 70);
  EXPECT_EQ(second.GetAccessCount(), 0);
  EXPECT_EQ(second.GetCacheMissCount(), 0);
  EXPECT_EQ(second.LookUpUpdate(7), 700);
  EXPECT_EQ(first.LookUpUpdate(7), 70);
  EXPECT_EQ(first.GetCacheMissCount(), 1);
  EXPECT_EQ(first.GetAccessCount(), 2);
  EXPECT_EQ(second.GetCacheMissCount(), 1);
  EXPECT_EQ(second.GetAccessCount(), 1);
}

TEST(LruCacheTest, HoldsTwoPages) {
  cache::Lru<int, int> cache(2, LoadPage);

  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.GetCacheMissCount(), 2);

  EXPECT_EQ(cache.LookUpUpdate(3), 30);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 4);
  EXPECT_EQ(cache.GetAccessCount(), 5);
}

// LRU cache: a requested capacity of zero is increased to one.
TEST(LruCacheTest, ZeroCapacityIsClampedToOne) {
  cache::Lru<int, int> cache(0, LoadPage);

  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 1);

  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 3);
  EXPECT_EQ(cache.GetAccessCount(), 4);
}

TEST(LruCacheTest, HitProtectsLeastRecentPageFromEviction) {
  cache::Lru<int, int> cache(2, LoadPage);
  cache.LookUpUpdate(1);
  cache.LookUpUpdate(2);
  cache.LookUpUpdate(1);
  cache.LookUpUpdate(3);

  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.LookUpUpdate(3), 30);
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.GetCacheMissCount(), 4);
  EXPECT_EQ(cache.GetAccessCount(), 7);
}

TEST(LruCacheTest, EvictedPageReloadsFreshValue) {
  int version = 0;
  auto loader = [&](const int&) { return ++version; };
  cache::Lru<int, int> cache(1, loader);

  EXPECT_EQ(cache.LookUpUpdate(10), 1);
  EXPECT_EQ(cache.LookUpUpdate(20), 2);
  EXPECT_EQ(cache.LookUpUpdate(10), 3);
  EXPECT_EQ(cache.LookUpUpdate(10), 3);
  EXPECT_EQ(version, 3);
  EXPECT_EQ(cache.GetAccessCount(), 4);
  EXPECT_EQ(cache.GetCacheMissCount(), 3);
}


// === LRU cache: corner case with 50 accesses ===
TEST(LruCacheTest, FiftyAccessesWithChangingWorkingSet) {
  const std::array<int, 50> keys = {
      // Fill the cache, refresh pages and overflow capacity.
      1, 2, 3, 4, 1, 2, 5, 1, 2, 3,
      // Reuse surviving pages, then introduce page 6.
      4, 5, 3, 4, 5, 6, 3, 4, 5, 6,
      // Replace the working set with pages 7 through 10.
      7, 8, 9, 10, 7, 8, 9, 10, 7, 8,
      // Return to the original working set.
      1, 2, 1, 2, 3, 4, 1, 2, 3, 4,
      // Keep page 4 hot while replacing its neighbours.
      4, 4, 5, 4, 6, 4, 7, 4, 5, 6,
  };
  // Cumulative misses after each block of ten accesses.
  const std::array<size_t, 5> expected_misses = {6, 9, 13, 17, 20};
  size_t loader_calls = 0;
  auto loader = [&](const int& key) {
    ++loader_calls;
    return LoadPage(key);
  };
  cache::Lru<int, int> cache(4, loader);

  for (size_t i = 0; i < keys.size(); ++i) {
    SCOPED_TRACE(testing::Message() << "access=" << i + 1
                                  << ", key=" << keys[i]);
    EXPECT_EQ(cache.LookUpUpdate(keys[i]), LoadPage(keys[i]));
    EXPECT_EQ(cache.GetAccessCount(), i + 1);
    if ((i + 1) % 10 == 0) {
      EXPECT_EQ(cache.GetCacheMissCount(), expected_misses[i / 10]);
      EXPECT_EQ(loader_calls, expected_misses[i / 10]);
    }
  }
}

// Expected versions and misses were calculated with a separate simulation.
TEST(LruCacheTest, HundredTwentyAccessesWithScansAndReverseTraversal) {
  // Each row is {key, loaded value version, cumulative misses}.
  const std::array<ExpectedAccess, 120> accesses = {{
      // Fill the cache and refresh selected pages before each overflow.
      {1, 1, 1}, {2, 2, 2}, {3, 3, 3}, {4, 4, 4},
      {1, 1, 4}, {2, 2, 4}, {5, 5, 5}, {1, 1, 5},
      {2, 2, 5}, {3, 6, 6}, {4, 7, 7}, {5, 8, 8},
      {3, 6, 8}, {4, 7, 8}, {5, 8, 8}, {6, 9, 9},
      {3, 6, 9}, {4, 7, 9}, {5, 8, 9}, {6, 9, 9},
      // Reverse a fitting working set, then keep page 7 hot during replacements.
      {7, 10, 10}, {8, 11, 11}, {9, 12, 12}, {10, 13, 13},
      {7, 10, 13}, {8, 11, 13}, {9, 12, 13}, {10, 13, 13},
      {10, 13, 13}, {9, 12, 13}, {8, 11, 13}, {7, 10, 13},
      {11, 14, 14}, {7, 10, 14}, {12, 15, 15}, {7, 10, 15},
      {13, 16, 16}, {7, 10, 16}, {11, 14, 16}, {12, 15, 16},
      // Return to old keys and interleave page 4 with new neighbours.
      {1, 17, 17}, {2, 18, 18}, {1, 17, 18}, {2, 18, 18},
      {3, 19, 19}, {4, 20, 20}, {1, 17, 20}, {2, 18, 20},
      {3, 19, 20}, {4, 20, 20}, {4, 20, 20}, {4, 20, 20},
      {5, 21, 21}, {4, 20, 21}, {6, 22, 22}, {4, 20, 22},
      {7, 23, 23}, {4, 20, 23}, {5, 21, 23}, {6, 22, 23},
      // Scan ten new keys and traverse them in reverse order.
      {20, 24, 24}, {21, 25, 25}, {22, 26, 26}, {23, 27, 27},
      {24, 28, 28}, {25, 29, 29}, {26, 30, 30}, {27, 31, 31},
      {28, 32, 32}, {29, 33, 33}, {29, 33, 33}, {28, 32, 33},
      {27, 31, 33}, {26, 30, 33}, {25, 34, 34}, {24, 35, 35},
      {23, 36, 36}, {22, 37, 37}, {21, 38, 38}, {20, 39, 39},
      // Mix zero and negative keys while crossing the capacity boundary.
      {0, 40, 40}, {-1, 41, 41}, {-2, 42, 42}, {-3, 43, 43},
      {0, 40, 43}, {-1, 41, 43}, {-2, 42, 43}, {-3, 43, 43},
      {-4, 44, 44}, {-3, 43, 44}, {-2, 42, 44}, {-1, 41, 44},
      {0, 45, 45}, {-4, 46, 46}, {-3, 47, 47}, {-2, 48, 48},
      {-1, 49, 49}, {0, 50, 50}, {0, 50, 50}, {0, 50, 50},
      // Alternate fitting and oversized cycles, then reverse direction again.
      {1, 51, 51}, {2, 52, 52}, {3, 53, 53}, {1, 51, 53},
      {2, 52, 53}, {3, 53, 53}, {4, 54, 54}, {5, 55, 55},
      {1, 56, 56}, {2, 57, 57}, {3, 58, 58}, {4, 59, 59},
      {5, 60, 60}, {5, 60, 60}, {4, 59, 60}, {3, 58, 60},
      {2, 57, 60}, {1, 61, 61}, {2, 57, 61}, {1, 61, 61},
  }};
  int loader_calls = 0;
  auto loader = [&](const int&) { return ++loader_calls; };
  cache::Lru<int, int> cache(4, loader);

  for (size_t i = 0; i < accesses.size(); ++i) {
    const auto& access = accesses[i];
    SCOPED_TRACE(testing::Message() << "access=" << i + 1
                                  << ", key=" << access.key);
    EXPECT_EQ(cache.LookUpUpdate(access.key), access.value);
    EXPECT_EQ(cache.GetCacheMissCount(), access.misses);
    EXPECT_EQ(loader_calls, access.misses);
    EXPECT_EQ(cache.GetAccessCount(), i + 1);
  }
}

TEST(LruCacheTest, SixtyMixedAccessesAcrossCapacities) {
  const std::array<int, 60> keys = {
      // Reuse zero and negative keys while filling the cache.
      0, -1, 0, 1, 2, -1, 0, 3, 1, 2,
      // Interleave new pages with recently evicted pages.
      4, 5, 3, 4, 6, 5, 7, 6, 8, 7,
      // Return to a small working set and hit it repeatedly.
      0, -1, 0, -1, 1, 2, 1, 2, 3, 3,
      // Scan ten distinct pages to overflow small caches and their histories.
      10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
      // Reverse the end of the scan, then return to the original working set.
      19, 18, 17, 16, 0, -1, 0, -1, 1, 2,
      // Alternate two small sets before revisiting the oldest keys.
      3, 4, 3, 4, 5, 6, 5, 6, 0, -1,
  };
  const std::array<size_t, 7> capacities = {0, 1, 2, 3, 4, 8, 20};
  // Independently simulated outcomes: M = load, H = resident hit.
  // Each string corresponds to the capacity at the same index above.
  const std::array<std::string, 7> expected_outcomes = {
      "MMMMMMMMMM" "MMMMMMMMMM" "MMMMMMMMMH" "MMMMMMMMMM" "HMMMMMMMMM" "MMMMMMMMMM",
      "MMMMMMMMMM" "MMMMMMMMMM" "MMMMMMMMMH" "MMMMMMMMMM" "HMMMMMMMMM" "MMMMMMMMMM",
      "MMHMMMMMMM" "MMMMMMMMMM" "MMHHMMHHMH" "MMMMMMMMMM" "HHMMMMHHMM" "MMHHMMHHMM",
      "MMHMMMMMMM" "MMMHMMMHMH" "MMHHMMHHMH" "MMMMMMMMMM" "HHHMMMHHMM" "MMHHMMHHMM",
      "MMHMMHHMMM" "MMMHMHMHMH" "MMHHMMHHMH" "MMMMMMMMMM" "HHHHMMHHMM" "MMHHMMHHMM",
      "MMHMMHHMHH" "MMHHMHMHMH" "MMHHMMHHMH" "MMMMMMMMMM" "HHHHMMHHMM" "MMHHMMHHHH",
      "MMHMMHHMHH" "MMHHMHMHMH" "HHHHHHHHHH" "MMMMMMMMMM" "HHHHHHHHHH" "HHHHHHHHHH",
  };

  for (size_t scenario = 0; scenario < capacities.size(); ++scenario) {
    SCOPED_TRACE(testing::Message() << "capacity=" << capacities[scenario]);
    ASSERT_EQ(expected_outcomes[scenario].size(), keys.size());
    size_t loader_calls = 0;
    size_t expected_misses = 0;
    auto loader = [&](const int& key) {
      ++loader_calls;
      return LoadPage(key);
    };
    cache::Lru<int, int> cache(capacities[scenario], loader);

    for (size_t i = 0; i < keys.size(); ++i) {
      SCOPED_TRACE(testing::Message() << "access=" << i + 1
                                    << ", key=" << keys[i]);
      if (expected_outcomes[scenario][i] == 'M') {
        ++expected_misses;
      }
      EXPECT_EQ(cache.LookUpUpdate(keys[i]), LoadPage(keys[i]));
      EXPECT_EQ(cache.GetCacheMissCount(), expected_misses);
      EXPECT_EQ(loader_calls, expected_misses);
      EXPECT_EQ(cache.GetAccessCount(), i + 1);
    }
  }
}

// ============================================================================
// === TwoQueues cache ===
// ============================================================================
// Stores new pages in IN and reused pages from OUT in LRU.
TEST(TwoQueuesTest, ConstructorInitializesCountersToZero) {
  int loader_calls = 0;
  cache::TwoQueues<int, int> cache(20, [&](const int& key) {
    ++loader_calls;
    return LoadPage(key);
  });

  EXPECT_EQ(loader_calls, 0);
  EXPECT_EQ(cache.GetCacheMissCount(), 0);
  EXPECT_EQ(cache.GetAccessCount(), 0);
}

TEST(TwoQueuesTest, FirstLookupLoadsPage) {
  int loader_calls = 0;
  cache::TwoQueues<int, int> cache(20, [&](const int& key) {
    ++loader_calls;
    EXPECT_EQ(key, 7);
    return LoadPage(key);
  });

  EXPECT_EQ(cache.LookUpUpdate(7), 70);
  EXPECT_EQ(loader_calls, 1);
  EXPECT_EQ(cache.GetCacheMissCount(), 1);
  EXPECT_EQ(cache.GetAccessCount(), 1);
}

TEST(TwoQueuesTest, RepeatedLookupUsesCachedPage) {
  int loader_calls = 0;
  cache::TwoQueues<int, int> cache(20, [&](const int& key) {
    return LoadPage(key) * ++loader_calls;
  });

  EXPECT_EQ(cache.LookUpUpdate(7), 70);
  EXPECT_EQ(cache.LookUpUpdate(7), 70);
  EXPECT_EQ(loader_calls, 1);
  EXPECT_EQ(cache.GetCacheMissCount(), 1);
  EXPECT_EQ(cache.GetAccessCount(), 2);
}

TEST(TwoQueuesTest, InstancesHaveIndependentState) {
  cache::TwoQueues<int, int> first(20, LoadPage);
  cache::TwoQueues<int, int> second(20, [](const int& key) { return key * 100; });

  EXPECT_EQ(first.LookUpUpdate(7), 70);
  EXPECT_EQ(second.GetAccessCount(), 0);
  EXPECT_EQ(second.GetCacheMissCount(), 0);
  EXPECT_EQ(second.LookUpUpdate(7), 700);
  EXPECT_EQ(first.LookUpUpdate(7), 70);
  EXPECT_EQ(first.GetCacheMissCount(), 1);
  EXPECT_EQ(first.GetAccessCount(), 2);
  EXPECT_EQ(second.GetCacheMissCount(), 1);
  EXPECT_EQ(second.GetAccessCount(), 1);
}

TEST(TwoQueuesTest, InQueueHoldsTwoPages) {
  cache::TwoQueues<int, int> cache(20, LoadPage);

  // IN uses 10% of the capacity, so a cache of size 20 holds 2 pages in IN.
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.GetCacheMissCount(), 2);

  EXPECT_EQ(cache.LookUpUpdate(3), 30);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 4);
  EXPECT_EQ(cache.GetAccessCount(), 5);
}

TEST(TwoQueuesTest, InHitsDoNotChangeFifoEvictionOrder) {
  int loader_calls = 0;
  cache::TwoQueues<int, int> cache(20, [&](const int&) {
    return ++loader_calls;
  });  // IN holds two pages.
  EXPECT_EQ(cache.LookUpUpdate(1), 1);
  EXPECT_EQ(cache.LookUpUpdate(2), 2);
  EXPECT_EQ(cache.LookUpUpdate(1), 1);
  EXPECT_EQ(cache.LookUpUpdate(3), 3);

  EXPECT_EQ(cache.LookUpUpdate(2), 2);
  EXPECT_EQ(cache.LookUpUpdate(1), 4);
  EXPECT_EQ(loader_calls, 4);
  EXPECT_EQ(cache.GetCacheMissCount(), 4);
  EXPECT_EQ(cache.GetAccessCount(), 6);
}

TEST(TwoQueuesTest, OutHitReloadsAndPromotesPage) {
  int loader_calls = 0;
  cache::TwoQueues<int, int> cache(10, [&](const int&) {
    return ++loader_calls;
  });  // IN holds one page.
  EXPECT_EQ(cache.LookUpUpdate(1), 1);
  EXPECT_EQ(cache.LookUpUpdate(2), 2);
  EXPECT_EQ(cache.LookUpUpdate(1), 3);

  for (int key : {3, 4}) {
    cache.LookUpUpdate(key);
  }
  EXPECT_EQ(cache.LookUpUpdate(1), 3);
  EXPECT_EQ(loader_calls, 5);
  EXPECT_EQ(cache.GetCacheMissCount(), 5);
  EXPECT_EQ(cache.GetAccessCount(), 6);
}

TEST(TwoQueuesTest, ForgottenOutPageReturnsToInInsteadOfLru) {
  int loader_calls = 0;
  cache::TwoQueues<int, int> cache(10, [&](const int&) {
    return ++loader_calls;
  });  // OUT holds three keys.
  for (int key = 1; key <= 5; ++key) {
    EXPECT_EQ(cache.LookUpUpdate(key), key);
  }
  // Key 1 has left OUT, so it must enter IN on the next access.
  EXPECT_EQ(cache.LookUpUpdate(1), 6);
  EXPECT_EQ(cache.LookUpUpdate(6), 7);
  EXPECT_EQ(cache.LookUpUpdate(1), 8);
  EXPECT_EQ(loader_calls, 8);
  EXPECT_EQ(cache.GetCacheMissCount(), 8);
  EXPECT_EQ(cache.GetAccessCount(), 8);
}

TEST(TwoQueuesTest, LruHitChangesVictimWhenPromotionFillsLru) {
  int loader_calls = 0;
  cache::TwoQueues<int, int> cache(5, [&](const int&) {
    return ++loader_calls;
  });  // IN = 1, OUT = 1, LRU = 3.
  for (int key : {1, 2, 1, 3, 2, 4, 3}) {
    cache.LookUpUpdate(key);
  }
  // LRU contains 1, 2, 3. Touching 1 makes 2 the next victim.
  EXPECT_EQ(cache.LookUpUpdate(1), 3);
  EXPECT_EQ(cache.LookUpUpdate(5), 8);
  EXPECT_EQ(cache.LookUpUpdate(4), 9);
  EXPECT_EQ(cache.LookUpUpdate(1), 3);
  EXPECT_EQ(cache.LookUpUpdate(3), 7);
  EXPECT_EQ(cache.LookUpUpdate(4), 9);
  EXPECT_EQ(cache.LookUpUpdate(2), 10);
  EXPECT_EQ(loader_calls, 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 10);
  EXPECT_EQ(cache.GetAccessCount(), 14);
}

TEST(TwoQueuesTest, SmallCapacitiesKeepEachQueueUsable) {
  for (size_t capacity : {0, 1, 2}) {
    SCOPED_TRACE(capacity);
    cache::TwoQueues<int, int> cache(capacity, LoadPage);
    for (int key : {1, 2, 1, 3, 2}) {
      EXPECT_EQ(cache.LookUpUpdate(key), LoadPage(key));
    }
    EXPECT_EQ(cache.LookUpUpdate(2), 20);
    EXPECT_EQ(cache.LookUpUpdate(3), 30);
    EXPECT_EQ(cache.LookUpUpdate(1), 10);
    EXPECT_EQ(cache.GetCacheMissCount(), 6);
    EXPECT_EQ(cache.GetAccessCount(), 8);
  }
}


// === TwoQueues cache: corner case with 50 accesses ===
TEST(TwoQueuesTest, FiftyAccessesWithChangingWorkingSet) {
  const std::array<int, 50> keys = {
      // Fill LRU through OUT hits; IN and OUT each hold one page.
      1, 2, 1, 3, 2, 4, 3, 1, 2, 3,
      // Scan new pages, reuse LRU, then revisit a forgotten OUT key.
      5, 6, 7, 8, 9, 10, 1, 2, 3, 9,
      // Promote pages from OUT until the old LRU set is replaced.
      1, 2, 3, 10, 11, 10, 12, 11, 13, 12,
      // Overflow OUT while reusing the new LRU set.
      10, 11, 12, 14, 15, 16, 10, 11, 12, 15,
      // Alternate new pages, ghost promotions and LRU hits.
      17, 16, 18, 17, 19, 18, 16, 17, 18, 19,
  };
  // Cumulative misses after each block of ten accesses.
  const std::array<size_t, 5> expected_misses = {7, 14, 21, 25, 31};
  size_t loader_calls = 0;
  auto loader = [&](const int& key) {
    ++loader_calls;
    return LoadPage(key);
  };
  cache::TwoQueues<int, int> cache(5, loader);

  for (size_t i = 0; i < keys.size(); ++i) {
    SCOPED_TRACE(testing::Message() << "access=" << i + 1
                                  << ", key=" << keys[i]);
    EXPECT_EQ(cache.LookUpUpdate(keys[i]), LoadPage(keys[i]));
    EXPECT_EQ(cache.GetAccessCount(), i + 1);
    if ((i + 1) % 10 == 0) {
      EXPECT_EQ(cache.GetCacheMissCount(), expected_misses[i / 10]);
      EXPECT_EQ(loader_calls, expected_misses[i / 10]);
    }
  }
}

// Expected versions and misses were calculated with a separate simulation.
TEST(TwoQueuesTest, HundredTwentyAccessesWithQueueOverflowAndRepeatedPromotions) {
  // Each row is {key, loaded value version, cumulative misses}.
  const std::array<ExpectedAccess, 120> accesses = {{
      // IN = 2, OUT = 6, LRU = 12; IN hits preserve FIFO order.
      {1, 1, 1}, {2, 2, 2}, {1, 1, 2}, {3, 3, 3},
      {4, 4, 4}, {2, 5, 5}, {1, 6, 6}, {5, 7, 7},
      {6, 8, 8}, {3, 9, 9}, {4, 10, 10}, {7, 11, 11},
      {8, 12, 12}, {5, 13, 13}, {6, 14, 14}, {1, 6, 14},
      {2, 5, 14}, {3, 9, 14}, {4, 10, 14}, {5, 13, 14},
      // Promote OUT keys until LRU is full, then refresh selected LRU pages.
      {9, 15, 15}, {10, 16, 16}, {7, 17, 17}, {8, 18, 18},
      {11, 19, 19}, {12, 20, 20}, {9, 21, 21}, {10, 22, 22},
      {13, 23, 23}, {14, 24, 24}, {11, 25, 25}, {12, 26, 26},
      {1, 6, 26}, {3, 9, 26}, {5, 13, 26}, {7, 17, 26},
      {9, 21, 26}, {11, 25, 26}, {2, 5, 26}, {4, 10, 26},
      // Overflow LRU through repeated promotions and revisit its survivors.
      {15, 27, 27}, {16, 28, 28}, {13, 29, 29}, {14, 30, 30},
      {1, 6, 30}, {3, 9, 30}, {5, 13, 30}, {7, 17, 30},
      {9, 21, 30}, {11, 25, 30}, {17, 31, 31}, {18, 32, 32},
      {15, 33, 33}, {16, 34, 34}, {1, 6, 34}, {3, 9, 34},
      {19, 35, 35}, {20, 36, 36}, {17, 37, 37}, {18, 38, 38},
      // Overflow OUT with a scan, then mix forgotten keys and ghost hits.
      {21, 39, 39}, {22, 40, 40}, {23, 41, 41}, {24, 42, 42},
      {25, 43, 43}, {26, 44, 44}, {27, 45, 45}, {28, 46, 46},
      {29, 47, 47}, {30, 48, 48}, {1, 6, 48}, {3, 9, 48},
      {15, 33, 48}, {16, 34, 48}, {21, 49, 49}, {22, 50, 50},
      {29, 51, 51}, {30, 52, 52}, {21, 49, 52}, {22, 50, 52},
      // Promote more OUT pages while refreshing a small LRU working set.
      {31, 53, 53}, {32, 54, 54}, {29, 51, 54}, {30, 52, 54},
      {33, 55, 55}, {34, 56, 56}, {31, 57, 57}, {32, 58, 58},
      {1, 6, 58}, {3, 9, 58}, {35, 59, 59}, {36, 60, 60},
      {33, 61, 61}, {34, 62, 62}, {37, 63, 63}, {38, 64, 64},
      {35, 65, 65}, {36, 66, 66}, {1, 6, 66}, {3, 9, 66},
      // Repeat IN/OUT transitions with zero and negative keys, then revisit LRU.
      {0, 67, 67}, {-1, 68, 68}, {0, 67, 68}, {-2, 69, 69},
      {-3, 70, 70}, {-1, 71, 71}, {0, 72, 72}, {-4, 73, 73},
      {-5, 74, 74}, {-2, 75, 75}, {-3, 76, 76}, {1, 6, 76},
      {3, 9, 76}, {29, 77, 77}, {30, 78, 78}, {31, 57, 78},
      {32, 58, 78}, {33, 61, 78}, {34, 62, 78}, {35, 65, 78},
  }};
  int loader_calls = 0;
  auto loader = [&](const int&) { return ++loader_calls; };
  cache::TwoQueues<int, int> cache(20, loader);

  for (size_t i = 0; i < accesses.size(); ++i) {
    const auto& access = accesses[i];
    SCOPED_TRACE(testing::Message() << "access=" << i + 1
                                  << ", key=" << access.key);
    EXPECT_EQ(cache.LookUpUpdate(access.key), access.value);
    EXPECT_EQ(cache.GetCacheMissCount(), access.misses);
    EXPECT_EQ(loader_calls, access.misses);
    EXPECT_EQ(cache.GetAccessCount(), i + 1);
  }
}

TEST(TwoQueuesTest, SixtyMixedAccessesAcrossCapacities) {
  const std::array<int, 60> keys = {
      // Reuse zero and negative keys while filling the cache.
      0, -1, 0, 1, 2, -1, 0, 3, 1, 2,
      // Interleave new pages with recently evicted pages.
      4, 5, 3, 4, 6, 5, 7, 6, 8, 7,
      // Return to a small working set and hit it repeatedly.
      0, -1, 0, -1, 1, 2, 1, 2, 3, 3,
      // Scan ten distinct pages to overflow small caches and their histories.
      10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
      // Reverse the end of the scan, then return to the original working set.
      19, 18, 17, 16, 0, -1, 0, -1, 1, 2,
      // Alternate two small sets before revisiting the oldest keys.
      3, 4, 3, 4, 5, 6, 5, 6, 0, -1,
  };
  const std::array<size_t, 7> capacities = {0, 1, 2, 3, 4, 8, 20};
  // Independently simulated outcomes: M = load, H = resident hit.
  // Each string corresponds to the capacity at the same index above.
  const std::array<std::string, 7> expected_outcomes = {
      "MMMMMMHMMM" "MMMMMMMMMM" "HMHHMMMHMH" "MMMMMMMMMM" "HMMMMMMHMM" "MMMHMMMHMM",
      "MMMMMMHMMM" "MMMMMMMMMM" "HMHHMMMHMH" "MMMMMMMMMM" "HMMMMMMHMM" "MMMHMMMHMM",
      "MMMMMMHMMM" "MMMMMMMMMM" "HMHHMMMHMH" "MMMMMMMMMM" "HMMMMMMHMM" "MMMHMMMHMM",
      "MMMMMMHMMM" "MMMMMMMMMM" "HMHHMMMHMH" "MMMMMMMMMM" "HMMMMMMHMM" "MMMHMMMHMM",
      "MMMMMMHMMM" "MMMMMMMMMM" "HMHHMMMHMH" "MMMMMMMMMM" "HMMMMMMHMM" "MMMHMMMHMM",
      "MMMMMMHMMM" "MMMMMMMMMM" "MMMHMMMHMH" "MMMMMMMMMM" "HMMMHMHHHM" "MMMHMMMHHM",
      "MMHMMMMMMH" "MMMHMHMHMH" "HHHHHMHHHH" "MMMMMMMMMM" "HHMMHHHHHH" "HMHHMMHHHH",
  };

  for (size_t scenario = 0; scenario < capacities.size(); ++scenario) {
    SCOPED_TRACE(testing::Message() << "capacity=" << capacities[scenario]);
    ASSERT_EQ(expected_outcomes[scenario].size(), keys.size());
    size_t loader_calls = 0;
    size_t expected_misses = 0;
    auto loader = [&](const int& key) {
      ++loader_calls;
      return LoadPage(key);
    };
    cache::TwoQueues<int, int> cache(capacities[scenario], loader);

    for (size_t i = 0; i < keys.size(); ++i) {
      SCOPED_TRACE(testing::Message() << "access=" << i + 1
                                    << ", key=" << keys[i]);
      if (expected_outcomes[scenario][i] == 'M') {
        ++expected_misses;
      }
      EXPECT_EQ(cache.LookUpUpdate(keys[i]), LoadPage(keys[i]));
      EXPECT_EQ(cache.GetCacheMissCount(), expected_misses);
      EXPECT_EQ(loader_calls, expected_misses);
      EXPECT_EQ(cache.GetAccessCount(), i + 1);
    }
  }
}

// ============================================================================
// === ARC cache ===
// ============================================================================
// T1/T2 store pages; B1/B2 keep only the keys of evicted pages.
// === ARC cache: basic operations ===
TEST(ArcCacheTest, ConstructorInitializesCountersToZero) {
  int loader_calls = 0;
  cache::Arc<int, int> cache(2, [&](const int& key) {
    ++loader_calls;
    return LoadPage(key);
  });

  EXPECT_EQ(loader_calls, 0);
  EXPECT_EQ(cache.GetCacheMissCount(), 0);
  EXPECT_EQ(cache.GetAccessCount(), 0);
}

TEST(ArcCacheTest, FirstLookupLoadsPage) {
  int loader_calls = 0;
  cache::Arc<int, int> cache(2, [&](const int& key) {
    ++loader_calls;
    EXPECT_EQ(key, 7);
    return LoadPage(key);
  });

  EXPECT_EQ(cache.LookUpUpdate(7), 70);
  EXPECT_EQ(loader_calls, 1);
  EXPECT_EQ(cache.GetCacheMissCount(), 1);
  EXPECT_EQ(cache.GetAccessCount(), 1);
}

TEST(ArcCacheTest, RepeatedLookupUsesCachedPageInBothResidentQueues) {
  int loader_calls = 0;
  cache::Arc<int, int> cache(2, [&](const int& key) {
    return LoadPage(key) * ++loader_calls;
  });

  EXPECT_EQ(cache.LookUpUpdate(7), 70);
  // The first hit moves the page from T1 to T2; the next hit stays in T2.
  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ(cache.LookUpUpdate(7), 70);
  }
  EXPECT_EQ(loader_calls, 1);
  EXPECT_EQ(cache.GetCacheMissCount(), 1);
  EXPECT_EQ(cache.GetAccessCount(), 3);
}

TEST(ArcCacheTest, InstancesHaveIndependentState) {
  cache::Arc<int, int> first(2, LoadPage);
  cache::Arc<int, int> second(2, [](const int& key) { return key * 100; });

  EXPECT_EQ(first.LookUpUpdate(7), 70);
  EXPECT_EQ(second.GetAccessCount(), 0);
  EXPECT_EQ(second.GetCacheMissCount(), 0);
  EXPECT_EQ(second.LookUpUpdate(7), 700);
  EXPECT_EQ(first.LookUpUpdate(7), 70);
  EXPECT_EQ(first.GetCacheMissCount(), 1);
  EXPECT_EQ(first.GetAccessCount(), 2);
  EXPECT_EQ(second.GetCacheMissCount(), 1);
  EXPECT_EQ(second.GetAccessCount(), 1);
}

// === ARC cache: resident pages and eviction order ===
TEST(ArcCacheTest, HoldsTwoPages) {
  cache::Arc<int, int> cache(2, LoadPage);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 1);
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.GetCacheMissCount(), 2);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 2);
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.GetCacheMissCount(), 2);
  EXPECT_EQ(cache.GetAccessCount(), 4);
}

TEST(ArcCacheTest, FullT1EvictsOldestPage) {
  cache::Arc<int, int> cache(2, LoadPage);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 1);
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.GetCacheMissCount(), 2);
  EXPECT_EQ(cache.LookUpUpdate(3), 30);
  EXPECT_EQ(cache.GetCacheMissCount(), 3);
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.GetCacheMissCount(), 3);
  EXPECT_EQ(cache.LookUpUpdate(3), 30);
  EXPECT_EQ(cache.GetCacheMissCount(), 3);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 4);
  EXPECT_EQ(cache.GetAccessCount(), 6);
}

TEST(ArcCacheTest, T1HitProtectsPageFromEviction) {
  cache::Arc<int, int> cache(2, LoadPage);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 1);
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.GetCacheMissCount(), 2);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);  // Promote 1 to T2.
  EXPECT_EQ(cache.GetCacheMissCount(), 2);
  EXPECT_EQ(cache.LookUpUpdate(3), 30);   // Evict 2 from T1 to B1.
  EXPECT_EQ(cache.GetCacheMissCount(), 3);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 3);
  EXPECT_EQ(cache.LookUpUpdate(3), 30);
  EXPECT_EQ(cache.GetCacheMissCount(), 3);
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.GetCacheMissCount(), 4);
  EXPECT_EQ(cache.GetAccessCount(), 7);
}

TEST(ArcCacheTest, T2HitChangesLeastRecentlyUsedVictim) {
  cache::Arc<int, int> cache(2, LoadPage);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 1);
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.GetCacheMissCount(), 2);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 2);
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.GetCacheMissCount(), 2);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);  // T2 order is now 1, 2.
  EXPECT_EQ(cache.GetCacheMissCount(), 2);
  EXPECT_EQ(cache.LookUpUpdate(3), 30);   // Evict 2 to B2.
  EXPECT_EQ(cache.GetCacheMissCount(), 3);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 3);
  EXPECT_EQ(cache.LookUpUpdate(3), 30);
  EXPECT_EQ(cache.GetCacheMissCount(), 3);
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.GetCacheMissCount(), 4);
  EXPECT_EQ(cache.GetAccessCount(), 9);
}

// === ARC cache: ghost hits and adaptation ===
TEST(ArcCacheTest, B1HitGivesRecentPagesMoreSpace) {
  cache::Arc<int, int> cache(2, LoadPage);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 1);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 1);
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.GetCacheMissCount(), 2);
  // T1 = [3], T2 = [1], B1 = [2].
  EXPECT_EQ(cache.LookUpUpdate(3), 30);
  EXPECT_EQ(cache.GetCacheMissCount(), 3);
  // Increase p to 1 and evict 1 from T2.
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.GetCacheMissCount(), 4);
  EXPECT_EQ(cache.LookUpUpdate(3), 30);
  EXPECT_EQ(cache.GetCacheMissCount(), 4);
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.GetCacheMissCount(), 4);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 5);
  EXPECT_EQ(cache.GetAccessCount(), 8);
}

TEST(ArcCacheTest, B2HitGivesFrequentPagesMoreSpace) {
  cache::Arc<int, int> cache(2, LoadPage);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 1);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 1);
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.GetCacheMissCount(), 2);
  EXPECT_EQ(cache.LookUpUpdate(3), 30);
  EXPECT_EQ(cache.GetCacheMissCount(), 3);
  // p = 1, T1 = [3], T2 = [2], B2 = [1].
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.GetCacheMissCount(), 4);
  // Decrease p to 0 and evict 3 from T1.
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 5);
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.GetCacheMissCount(), 5);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 5);
  EXPECT_EQ(cache.LookUpUpdate(3), 30);
  EXPECT_EQ(cache.GetCacheMissCount(), 6);
  EXPECT_EQ(cache.GetAccessCount(), 9);
}

TEST(ArcCacheTest, GhostHitsReloadFreshValuesAndThenCacheThem) {
  int version = 0;
  auto loader = [&](const int&) { return ++version; };
  cache::Arc<int, int> cache(2, loader);
  EXPECT_EQ(cache.LookUpUpdate(1), 1);
  EXPECT_EQ(cache.LookUpUpdate(1), 1);
  EXPECT_EQ(cache.LookUpUpdate(2), 2);
  EXPECT_EQ(cache.LookUpUpdate(3), 3);
  EXPECT_EQ(cache.LookUpUpdate(2), 4);  // B1 hit.
  EXPECT_EQ(cache.LookUpUpdate(2), 4);
  EXPECT_EQ(cache.LookUpUpdate(1), 5);  // B2 hit.
  EXPECT_EQ(cache.LookUpUpdate(1), 5);
  EXPECT_EQ(version, 5);
  EXPECT_EQ(cache.GetCacheMissCount(), 5);
  EXPECT_EQ(cache.GetAccessCount(), 8);
}

TEST(ArcCacheTest, B2HitAtZeroTargetDoesNotUnderflow) {
  cache::Arc<int, int> cache(2, LoadPage);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 1);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 1);
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.GetCacheMissCount(), 2);
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.GetCacheMissCount(), 2);
  // p is still 0; 1 is now in B2.
  EXPECT_EQ(cache.LookUpUpdate(3), 30);
  EXPECT_EQ(cache.GetCacheMissCount(), 3);
  // Keep p at 0 and evict 3, preserving 2.
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 4);
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.GetCacheMissCount(), 4);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 4);
  EXPECT_EQ(cache.LookUpUpdate(3), 30);
  EXPECT_EQ(cache.GetCacheMissCount(), 5);
  EXPECT_EQ(cache.GetAccessCount(), 9);
}

// === ARC cache: scans and bounded ghost history ===
TEST(ArcCacheTest, FrequentPageSurvivesLongScan) {
  cache::Arc<int, int> cache(2, LoadPage);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 1);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 1);
  for (int key = 2; key <= 100; ++key) {
    EXPECT_EQ(cache.LookUpUpdate(key), LoadPage(key));
    EXPECT_EQ(cache.GetCacheMissCount(), static_cast<size_t>(key));
  }
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 100);
  EXPECT_EQ(cache.LookUpUpdate(100), 1000);
  EXPECT_EQ(cache.GetCacheMissCount(), 100);
  EXPECT_EQ(cache.GetAccessCount(), 103);
}

TEST(ArcCacheTest, ForgottenB1PageReturnsToT1) {
  cache::Arc<int, int> cache(2, LoadPage);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 1);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 1);
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.GetCacheMissCount(), 2);
  EXPECT_EQ(cache.LookUpUpdate(3), 30);  // B1 contains 2.
  EXPECT_EQ(cache.GetCacheMissCount(), 3);
  // Forget 2; B1 now contains 3.
  EXPECT_EQ(cache.LookUpUpdate(4), 40);
  EXPECT_EQ(cache.GetCacheMissCount(), 4);
  // A new page, so insert into T1.
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.GetCacheMissCount(), 5);
  // Evict 2, preserving the frequent page 1.
  EXPECT_EQ(cache.LookUpUpdate(5), 50);
  EXPECT_EQ(cache.GetCacheMissCount(), 6);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 6);
  EXPECT_EQ(cache.LookUpUpdate(2), 20);
  EXPECT_EQ(cache.GetCacheMissCount(), 7);
  EXPECT_EQ(cache.GetAccessCount(), 9);
}

TEST(ArcCacheTest, FullHistoryForgetsOldestB2Page) {
  cache::Arc<int, int> cache(2, LoadPage);
  for (int key = 1; key <= 4; ++key) {
    EXPECT_EQ(cache.LookUpUpdate(key), LoadPage(key));
    EXPECT_EQ(cache.GetCacheMissCount(), static_cast<size_t>(key));
    EXPECT_EQ(cache.LookUpUpdate(key), LoadPage(key));
    EXPECT_EQ(cache.GetCacheMissCount(), static_cast<size_t>(key));
  }
  // T2 = [4, 3], B2 = [2, 1]: the directory has reached 2 * capacity.
  // Forget 1 from B2 before replacing a page.
  EXPECT_EQ(cache.LookUpUpdate(5), 50);
  EXPECT_EQ(cache.GetCacheMissCount(), 5);
  // Forgotten 1 enters T1, not T2.
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 6);
  EXPECT_EQ(cache.LookUpUpdate(6), 60);  // Evict 1 from T1.
  EXPECT_EQ(cache.GetCacheMissCount(), 7);
  EXPECT_EQ(cache.LookUpUpdate(4), 40);
  EXPECT_EQ(cache.GetCacheMissCount(), 7);
  EXPECT_EQ(cache.LookUpUpdate(1), 10);
  EXPECT_EQ(cache.GetCacheMissCount(), 8);
  EXPECT_EQ(cache.GetAccessCount(), 13);
}

// === ARC cache: capacity boundaries ===
TEST(ArcCacheTest, WorkingSetFitsWithoutFurtherLoads) {
  for (size_t capacity : {1, 2, 3, 16}) {
    SCOPED_TRACE(capacity);
    size_t loader_calls = 0;
    auto loader = [&](const int& key) {
      ++loader_calls;
      return LoadPage(key);
    };
    cache::Arc<int, int> cache(capacity, loader);
    for (int round = 0; round < 10; ++round) {
      for (size_t key = 0; key < capacity; ++key) {
        EXPECT_EQ(cache.LookUpUpdate(static_cast<int>(key)),
                  LoadPage(static_cast<int>(key)));
      }
    }
    EXPECT_EQ(loader_calls, capacity);
    EXPECT_EQ(cache.GetCacheMissCount(), capacity);
    EXPECT_EQ(cache.GetAccessCount(), 10 * capacity);
  }
}

TEST(ArcCacheTest, ScanLargerThanCapacityMissesOnEveryAccess) {
  for (size_t capacity : {1, 2, 3, 16}) {
    SCOPED_TRACE(capacity);
    size_t loader_calls = 0;
    auto loader = [&](const int& key) {
      ++loader_calls;
      return LoadPage(key);
    };
    cache::Arc<int, int> cache(capacity, loader);
    for (int round = 0; round < 10; ++round) {
      for (size_t key = 0; key <= capacity; ++key) {
        EXPECT_EQ(cache.LookUpUpdate(static_cast<int>(key)),
                  LoadPage(static_cast<int>(key)));
      }
    }
    EXPECT_EQ(loader_calls, 10 * (capacity + 1));
    EXPECT_EQ(cache.GetCacheMissCount(), loader_calls);
    EXPECT_EQ(cache.GetAccessCount(), loader_calls);
  }
}

TEST(ArcCacheTest, AlternatingPromotedPagesReloadsGhosts) {
  int version = 0;
  auto loader = [&](const int&) { return ++version; };
  cache::Arc<int, int> cache(1, loader);
  for (int round = 0; round < 20; ++round) {
    const int key = round % 2;
    EXPECT_EQ(cache.LookUpUpdate(key), round + 1);
    EXPECT_EQ(cache.LookUpUpdate(key), round + 1);
  }
  EXPECT_EQ(version, 20);
  EXPECT_EQ(cache.GetCacheMissCount(), 20);
  EXPECT_EQ(cache.GetAccessCount(), 40);
}

// === ARC cache: corner case with 50 accesses ===
TEST(ArcCacheTest, FiftyAccessesWithChangingWorkingSet) {
  const std::array<int, 50> keys = {
      // Promote repeated pages into T2 and start filling B2.
      1, 1, 2, 2, 3, 3, 4, 4, 5, 5,
      // Create unequal ghost sizes and revisit both B1 and B2.
      6, 6, 7, 8, 7, 9, 10, 8, 7, 6,
      // Scan new pages, then switch to a small reused set.
      11, 12, 13, 14, 7, 8, 7, 8, 15, 16,
      // Return to old pages and rebuild a four-page working set.
      1, 2, 3, 4, 1, 2, 3, 4, 1, 2,
      // Switch again, mixing new pages and immediate reuse.
      5, 6, 5, 6, 7, 8, 5, 6, 7, 8,
  };
  // Cumulative misses after each block of ten accesses.
  const std::array<size_t, 5> expected_misses = {5, 12, 20, 27, 31};
  size_t loader_calls = 0;
  auto loader = [&](const int& key) {
    ++loader_calls;
    return LoadPage(key);
  };
  cache::Arc<int, int> cache(4, loader);

  for (size_t i = 0; i < keys.size(); ++i) {
    SCOPED_TRACE(testing::Message() << "access=" << i + 1
                                  << ", key=" << keys[i]);
    EXPECT_EQ(cache.LookUpUpdate(keys[i]), LoadPage(keys[i]));
    EXPECT_EQ(cache.GetAccessCount(), i + 1);
    if ((i + 1) % 10 == 0) {
      EXPECT_EQ(cache.GetCacheMissCount(), expected_misses[i / 10]);
      EXPECT_EQ(loader_calls, expected_misses[i / 10]);
    }
  }
}

// Expected versions and misses were calculated with a separate simulation.
TEST(ArcCacheTest, HundredTwentyAccessesWithAdaptationAndHistoryOverflow) {
  // Each row is {key, loaded value version, cumulative misses}.
  const std::array<ExpectedAccess, 120> accesses = {{
      // Fill T2 and B2, then use unequal ghost sizes to increase p by three.
      {1, 1, 1}, {1, 1, 1}, {2, 2, 2}, {2, 2, 2},
      {3, 3, 3}, {3, 3, 3}, {4, 4, 4}, {4, 4, 4},
      {5, 5, 5}, {5, 5, 5}, {6, 6, 6}, {6, 6, 6},
      {7, 7, 7}, {8, 8, 8}, {7, 9, 9}, {9, 10, 10},
      {10, 11, 11}, {8, 8, 11}, {7, 9, 11}, {6, 12, 12},
      // Overflow ghost history and revisit both recent and frequent pages.
      {11, 13, 13}, {12, 14, 14}, {13, 15, 15}, {14, 16, 16},
      {7, 17, 17}, {8, 18, 18}, {7, 17, 18}, {8, 18, 18},
      {15, 19, 19}, {16, 20, 20}, {1, 21, 21}, {2, 22, 22},
      {3, 23, 23}, {4, 24, 24}, {1, 25, 25}, {2, 26, 26},
      {3, 27, 27}, {4, 24, 27}, {1, 25, 27}, {2, 26, 27},
      // Change working sets while B1/B2 hits move p between zero and capacity.
      {5, 28, 28}, {6, 29, 29}, {5, 28, 29}, {6, 29, 29},
      {7, 30, 30}, {8, 31, 31}, {5, 28, 31}, {6, 29, 31},
      {7, 30, 31}, {8, 31, 31}, {9, 32, 32}, {10, 33, 33},
      {11, 34, 34}, {12, 35, 35}, {9, 36, 36}, {10, 37, 37},
      {11, 38, 38}, {12, 35, 38}, {9, 36, 38}, {10, 37, 38},
      // Revisit older sets and reverse the access order across replacements.
      {13, 39, 39}, {14, 40, 40}, {15, 41, 41}, {16, 42, 42},
      {13, 39, 42}, {14, 40, 42}, {15, 41, 42}, {16, 42, 42},
      {1, 43, 43}, {2, 44, 44}, {3, 45, 45}, {4, 46, 46},
      {1, 43, 46}, {2, 44, 46}, {3, 45, 46}, {4, 46, 46},
      {16, 47, 47}, {15, 48, 48}, {14, 49, 49}, {13, 50, 50},
      // Promote a stream of new pages and overflow the B2 history repeatedly.
      {20, 51, 51}, {20, 51, 51}, {21, 52, 52}, {21, 52, 52},
      {22, 53, 53}, {22, 53, 53}, {23, 54, 54}, {23, 54, 54},
      {24, 55, 55}, {24, 55, 55}, {25, 56, 56}, {25, 56, 56},
      {26, 57, 57}, {26, 57, 57}, {27, 58, 58}, {27, 58, 58},
      {28, 59, 59}, {28, 59, 59}, {29, 60, 60}, {29, 60, 60},
      // Rebuild the working set with zero and negative keys, then reverse it.
      {0, 61, 61}, {-1, 62, 62}, {-2, 63, 63}, {-3, 64, 64},
      {0, 65, 65}, {-1, 66, 66}, {-2, 67, 67}, {-3, 64, 67},
      {-4, 68, 68}, {-3, 64, 68}, {-2, 67, 68}, {-1, 66, 68},
      {0, 69, 69}, {-4, 68, 69}, {-3, 70, 70}, {-2, 71, 71},
      {-1, 72, 72}, {0, 73, 73}, {0, 73, 73}, {0, 73, 73},
  }};
  int loader_calls = 0;
  auto loader = [&](const int&) { return ++loader_calls; };
  cache::Arc<int, int> cache(4, loader);

  for (size_t i = 0; i < accesses.size(); ++i) {
    const auto& access = accesses[i];
    SCOPED_TRACE(testing::Message() << "access=" << i + 1
                                  << ", key=" << access.key);
    EXPECT_EQ(cache.LookUpUpdate(access.key), access.value);
    EXPECT_EQ(cache.GetCacheMissCount(), access.misses);
    EXPECT_EQ(loader_calls, access.misses);
    EXPECT_EQ(cache.GetAccessCount(), i + 1);
  }
}

TEST(ArcCacheTest, SixtyMixedAccessesAcrossCapacities) {
  const std::array<int, 60> keys = {
      // Reuse zero and negative keys while filling the cache.
      0, -1, 0, 1, 2, -1, 0, 3, 1, 2,
      // Interleave new pages with recently evicted pages.
      4, 5, 3, 4, 6, 5, 7, 6, 8, 7,
      // Return to a small working set and hit it repeatedly.
      0, -1, 0, -1, 1, 2, 1, 2, 3, 3,
      // Scan ten distinct pages to overflow small caches and their histories.
      10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
      // Reverse the end of the scan, then return to the original working set.
      19, 18, 17, 16, 0, -1, 0, -1, 1, 2,
      // Alternate two small sets before revisiting the oldest keys.
      3, 4, 3, 4, 5, 6, 5, 6, 0, -1,
  };
  const std::array<size_t, 6> capacities = {1, 2, 3, 4, 8, 20};
  // Independently simulated outcomes: M = load, H = resident hit.
  // Each string corresponds to the capacity at the same index above.
  const std::array<std::string, 6> expected_outcomes = {
      "MMMMMMMMMM" "MMMMMMMMMM" "MMMMMMMMMH" "MMMMMMMMMM" "HMMMMMMMMM" "MMMMMMMMMM",
      "MMHMMMHMMM" "MMMMMMMMMM" "HMHHMMMHMH" "MMMMMMMMMM" "HHMMMMHHMM" "MMHHMMHHMM",
      "MMHMMMHMMH" "MMHHMHMHMH" "MMHHMMHHMH" "MMMMMMMMMM" "HHHMMMHHMM" "MMHHMMHHMM",
      "MMHMMHHMMM" "MMHHMHMHMH" "MMHHMMHHMH" "MMMMMMMMMM" "HHHMMMHHMM" "MMHHMMHHMM",
      "MMHMMHHMHH" "MMHHMHMMMH" "MMHHMMHHMH" "MMMMMMMMMM" "HMMMMMHHMM" "MMHHMMMHHH",
      "MMHMMHHMHH" "MMHHMHMHMH" "HHHHHHHHHH" "MMMMMMMMMM" "HHHHHHHHHH" "HHHHHHHHHH",
  };

  for (size_t scenario = 0; scenario < capacities.size(); ++scenario) {
    SCOPED_TRACE(testing::Message() << "capacity=" << capacities[scenario]);
    ASSERT_EQ(expected_outcomes[scenario].size(), keys.size());
    size_t loader_calls = 0;
    size_t expected_misses = 0;
    auto loader = [&](const int& key) {
      ++loader_calls;
      return LoadPage(key);
    };
    cache::Arc<int, int> cache(capacities[scenario], loader);

    for (size_t i = 0; i < keys.size(); ++i) {
      SCOPED_TRACE(testing::Message() << "access=" << i + 1
                                    << ", key=" << keys[i]);
      if (expected_outcomes[scenario][i] == 'M') {
        ++expected_misses;
      }
      EXPECT_EQ(cache.LookUpUpdate(keys[i]), LoadPage(keys[i]));
      EXPECT_EQ(cache.GetCacheMissCount(), expected_misses);
      EXPECT_EQ(loader_calls, expected_misses);
      EXPECT_EQ(cache.GetAccessCount(), i + 1);
    }
  }
}

// ============================================================================
// === LIRS cache ===
// ============================================================================
TEST(LirsCacheTest, ConstructorInitializesCountersToZero) {
  int loader_calls = 0;
  cache::Lirs<int, int> cache(2, [&](const int& key) {
    ++loader_calls;
    return LoadPage(key);
  });

  EXPECT_EQ(loader_calls, 0);
  EXPECT_EQ(cache.GetCacheMissCount(), 0);
  EXPECT_EQ(cache.GetAccessCount(), 0);
}

TEST(LirsCacheTest, FirstLookupLoadsPage) {
  int loader_calls = 0;
  cache::Lirs<int, int> cache(2, [&](const int& key) {
    ++loader_calls;
    EXPECT_EQ(key, 7);
    return LoadPage(key);
  });

  EXPECT_EQ(cache.LookUpUpdate(7), 70);
  EXPECT_EQ(loader_calls, 1);
  EXPECT_EQ(cache.GetCacheMissCount(), 1);
  EXPECT_EQ(cache.GetAccessCount(), 1);
}

TEST(LirsCacheTest, RepeatedLookupDoesNotCallLoader) {
  int loader_calls = 0;
  cache::Lirs<int, int> cache(2, [&](const int& key) {
    return LoadPage(key) * ++loader_calls;
  });

  EXPECT_EQ(cache.LookUpUpdate(7), 70);
  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(cache.LookUpUpdate(7), 70);
  }
  EXPECT_EQ(loader_calls, 1);
  EXPECT_EQ(cache.GetCacheMissCount(), 1);
  EXPECT_EQ(cache.GetAccessCount(), 101);
}

TEST(LirsCacheTest, InstancesHaveIndependentState) {
  cache::Lirs<int, int> first(2, LoadPage);
  cache::Lirs<int, int> second(2, [](const int& key) { return key * 100; });

  EXPECT_EQ(first.LookUpUpdate(7), 70);
  EXPECT_EQ(second.GetAccessCount(), 0);
  EXPECT_EQ(second.GetCacheMissCount(), 0);
  EXPECT_EQ(second.LookUpUpdate(7), 700);
  EXPECT_EQ(first.LookUpUpdate(7), 70);
  EXPECT_EQ(first.GetCacheMissCount(), 1);
  EXPECT_EQ(first.GetAccessCount(), 2);
  EXPECT_EQ(second.GetCacheMissCount(), 1);
  EXPECT_EQ(second.GetAccessCount(), 1);
}

TEST(LirsCacheTest, WorkingSetFitsWithoutFurtherLoads) {
  for (size_t capacity : {1, 2, 3, 16}) {
    SCOPED_TRACE(capacity);
    size_t loader_calls = 0;
    auto loader = [&](const int& key) {
      ++loader_calls;
      return LoadPage(key);
    };
    cache::Lirs<int, int> cache(capacity, loader);
    for (int round = 0; round < 10; ++round) {
      for (size_t key = 0; key < capacity; ++key) {
        EXPECT_EQ(cache.LookUpUpdate(static_cast<int>(key)),
                  LoadPage(static_cast<int>(key)));
      }
    }
    EXPECT_EQ(loader_calls, capacity);
    EXPECT_EQ(cache.GetCacheMissCount(), capacity);
    EXPECT_EQ(cache.GetAccessCount(), 10 * capacity);
  }
}

TEST(LirsCacheTest, SupportsStringKeysAndValues) {
  int loader_calls = 0;
  cache::Lirs<std::string, std::string> cache(3, [&](const std::string& key) {
    ++loader_calls;
    return "value:" + key;
  });
  EXPECT_EQ(cache.LookUpUpdate(""), "value:");
  EXPECT_EQ(cache.LookUpUpdate("hello"), "value:hello");
  EXPECT_EQ(cache.LookUpUpdate("world"), "value:world");
  EXPECT_EQ(cache.LookUpUpdate("world"), "value:world");
  EXPECT_EQ(cache.LookUpUpdate("hello"), "value:hello");
  EXPECT_EQ(cache.LookUpUpdate(""), "value:");
  EXPECT_EQ(loader_calls, 3);
  EXPECT_EQ(cache.GetCacheMissCount(), 3);
  EXPECT_EQ(cache.GetAccessCount(), 6);
}

TEST(LirsCacheTest, LirPagesSurviveLongScan) {
  int loader_calls = 0;
  cache::Lirs<int, int> cache(4, [&](const int& key) {
    ++loader_calls;
    return LoadPage(key);
  });
  for (int key = 1; key <= 100; ++key) {
    ASSERT_EQ(cache.LookUpUpdate(key), LoadPage(key));
    ASSERT_EQ(cache.GetCacheMissCount(), static_cast<size_t>(key));
  }
  for (int key : {1, 2, 3, 100}) {
    EXPECT_EQ(cache.LookUpUpdate(key), LoadPage(key));
    EXPECT_EQ(cache.GetCacheMissCount(), 100);
  }
  EXPECT_EQ(loader_calls, 100);
  EXPECT_EQ(cache.GetAccessCount(), 104);
}

// Expected versions and misses below were calculated with a separate simulation.
TEST(LirsCacheTest, CapacityOneReloadsEvictedPageWithFreshValue) {
  // A single resident slot must retain the most recently loaded page.
  // Each row is {key, loaded value version, cumulative misses}.
  const std::array<ExpectedAccess, 6> accesses = {{
      {1, 1, 1}, {1, 1, 1}, {2, 2, 2}, {2, 2, 2},
      {1, 3, 3}, {1, 3, 3},
  }};
  int loader_calls = 0;
  auto loader = [&](const int&) { return ++loader_calls; };
  cache::Lirs<int, int> cache(1, loader);

  for (size_t i = 0; i < accesses.size(); ++i) {
    const auto& access = accesses[i];
    SCOPED_TRACE(testing::Message() << "access=" << i + 1
                                  << ", key=" << access.key);
    ASSERT_EQ(cache.LookUpUpdate(access.key), access.value);
    ASSERT_EQ(cache.GetCacheMissCount(), access.misses);
    ASSERT_EQ(loader_calls, access.misses);
    ASSERT_EQ(cache.GetAccessCount(), i + 1);
  }
}

TEST(LirsCacheTest, NewPageEvictsOldestResidentHir) {
  // Pages 1 through 3 are LIR; loading 5 evicts HIR page 4.
  // Each row is {key, loaded value version, cumulative misses}.
  const std::array<ExpectedAccess, 10> accesses = {{
      {1, 1, 1}, {2, 2, 2}, {3, 3, 3}, {4, 4, 4},
      {5, 5, 5}, {1, 1, 5}, {2, 2, 5}, {3, 3, 5},
      {5, 5, 5}, {4, 6, 6},
  }};
  int loader_calls = 0;
  auto loader = [&](const int&) { return ++loader_calls; };
  cache::Lirs<int, int> cache(4, loader);

  for (size_t i = 0; i < accesses.size(); ++i) {
    const auto& access = accesses[i];
    SCOPED_TRACE(testing::Message() << "access=" << i + 1
                                  << ", key=" << access.key);
    ASSERT_EQ(cache.LookUpUpdate(access.key), access.value);
    ASSERT_EQ(cache.GetCacheMissCount(), access.misses);
    ASSERT_EQ(loader_calls, access.misses);
    ASSERT_EQ(cache.GetAccessCount(), i + 1);
  }
}

TEST(LirsCacheTest, ResidentHirInStackIsPromotedWithoutReload) {
  // Promoting 4 demotes bottom LIR page 1, which is evicted by 5.
  // Each row is {key, loaded value version, cumulative misses}.
  const std::array<ExpectedAccess, 9> accesses = {{
      {1, 1, 1}, {2, 2, 2}, {3, 3, 3}, {4, 4, 4},
      {4, 4, 4}, {5, 5, 5}, {4, 4, 5}, {2, 2, 5},
      {3, 3, 5},
  }};
  int loader_calls = 0;
  auto loader = [&](const int&) { return ++loader_calls; };
  cache::Lirs<int, int> cache(4, loader);

  for (size_t i = 0; i < accesses.size(); ++i) {
    const auto& access = accesses[i];
    SCOPED_TRACE(testing::Message() << "access=" << i + 1
                                  << ", key=" << access.key);
    ASSERT_EQ(cache.LookUpUpdate(access.key), access.value);
    ASSERT_EQ(cache.GetCacheMissCount(), access.misses);
    ASSERT_EQ(loader_calls, access.misses);
    ASSERT_EQ(cache.GetAccessCount(), i + 1);
  }
}

TEST(LirsCacheTest, NonResidentHitReloadsFreshValueAndPromotes) {
  // Ghost page 4 reloads as LIR and survives the next HIR replacement.
  // Each row is {key, loaded value version, cumulative misses}.
  const std::array<ExpectedAccess, 10> accesses = {{
      {1, 1, 1}, {2, 2, 2}, {3, 3, 3}, {4, 4, 4},
      {5, 5, 5}, {4, 6, 6}, {6, 7, 7}, {4, 6, 7},
      {2, 2, 7}, {3, 3, 7},
  }};
  int loader_calls = 0;
  auto loader = [&](const int&) { return ++loader_calls; };
  cache::Lirs<int, int> cache(4, loader);

  for (size_t i = 0; i < accesses.size(); ++i) {
    const auto& access = accesses[i];
    SCOPED_TRACE(testing::Message() << "access=" << i + 1
                                  << ", key=" << access.key);
    ASSERT_EQ(cache.LookUpUpdate(access.key), access.value);
    ASSERT_EQ(cache.GetCacheMissCount(), access.misses);
    ASSERT_EQ(loader_calls, access.misses);
    ASSERT_EQ(cache.GetAccessCount(), i + 1);
  }
}

TEST(LirsCacheTest, LirHitChangesWhichPageIsDemoted) {
  // Refreshing LIR page 1 makes page 2 the next demotion victim.
  // Each row is {key, loaded value version, cumulative misses}.
  const std::array<ExpectedAccess, 10> accesses = {{
      {1, 1, 1}, {2, 2, 2}, {3, 3, 3}, {4, 4, 4},
      {1, 1, 4}, {4, 4, 4}, {5, 5, 5}, {1, 1, 5},
      {3, 3, 5}, {4, 4, 5},
  }};
  int loader_calls = 0;
  auto loader = [&](const int&) { return ++loader_calls; };
  cache::Lirs<int, int> cache(4, loader);

  for (size_t i = 0; i < accesses.size(); ++i) {
    const auto& access = accesses[i];
    SCOPED_TRACE(testing::Message() << "access=" << i + 1
                                  << ", key=" << access.key);
    ASSERT_EQ(cache.LookUpUpdate(access.key), access.value);
    ASSERT_EQ(cache.GetCacheMissCount(), access.misses);
    ASSERT_EQ(loader_calls, access.misses);
    ASSERT_EQ(cache.GetAccessCount(), i + 1);
  }
}

TEST(LirsCacheTest, PrunedResidentHirNeedsTwoHitsToPromote) {
  // Demoted 1 is outside the stack; its first hit must leave it HIR.
  // Each row is {key, loaded value version, cumulative misses}.
  const std::array<ExpectedAccess, 10> accesses = {{
      {1, 1, 1}, {2, 2, 2}, {3, 3, 3}, {4, 4, 4},
      {4, 4, 4}, {1, 1, 4}, {5, 5, 5}, {4, 4, 5},
      {2, 2, 5}, {3, 3, 5},
  }};
  int loader_calls = 0;
  auto loader = [&](const int&) { return ++loader_calls; };
  cache::Lirs<int, int> cache(4, loader);

  for (size_t i = 0; i < accesses.size(); ++i) {
    const auto& access = accesses[i];
    SCOPED_TRACE(testing::Message() << "access=" << i + 1
                                  << ", key=" << access.key);
    ASSERT_EQ(cache.LookUpUpdate(access.key), access.value);
    ASSERT_EQ(cache.GetCacheMissCount(), access.misses);
    ASSERT_EQ(loader_calls, access.misses);
    ASSERT_EQ(cache.GetAccessCount(), i + 1);
  }
}

TEST(LirsCacheTest, SecondHitPromotesPreviouslyPrunedHir) {
  // A second hit to demoted 1 promotes it and demotes page 2.
  // Each row is {key, loaded value version, cumulative misses}.
  const std::array<ExpectedAccess, 11> accesses = {{
      {1, 1, 1}, {2, 2, 2}, {3, 3, 3}, {4, 4, 4},
      {4, 4, 4}, {1, 1, 4}, {1, 1, 4}, {5, 5, 5},
      {1, 1, 5}, {3, 3, 5}, {4, 4, 5},
  }};
  int loader_calls = 0;
  auto loader = [&](const int&) { return ++loader_calls; };
  cache::Lirs<int, int> cache(4, loader);

  for (size_t i = 0; i < accesses.size(); ++i) {
    const auto& access = accesses[i];
    SCOPED_TRACE(testing::Message() << "access=" << i + 1
                                  << ", key=" << access.key);
    ASSERT_EQ(cache.LookUpUpdate(access.key), access.value);
    ASSERT_EQ(cache.GetCacheMissCount(), access.misses);
    ASSERT_EQ(loader_calls, access.misses);
    ASSERT_EQ(cache.GetAccessCount(), i + 1);
  }
}

TEST(LirsCacheTest, EvictedHirOutsideStackReturnsAsNewPage) {
  // Evicting demoted 1 must remove its directory entry before it returns.
  // Each row is {key, loaded value version, cumulative misses}.
  const std::array<ExpectedAccess, 11> accesses = {{
      {1, 1, 1}, {2, 2, 2}, {3, 3, 3}, {4, 4, 4},
      {4, 4, 4}, {5, 5, 5}, {1, 6, 6}, {6, 7, 7},
      {4, 4, 7}, {2, 2, 7}, {3, 3, 7},
  }};
  int loader_calls = 0;
  auto loader = [&](const int&) { return ++loader_calls; };
  cache::Lirs<int, int> cache(4, loader);

  for (size_t i = 0; i < accesses.size(); ++i) {
    const auto& access = accesses[i];
    SCOPED_TRACE(testing::Message() << "access=" << i + 1
                                  << ", key=" << access.key);
    ASSERT_EQ(cache.LookUpUpdate(access.key), access.value);
    ASSERT_EQ(cache.GetCacheMissCount(), access.misses);
    ASSERT_EQ(loader_calls, access.misses);
    ASSERT_EQ(cache.GetAccessCount(), i + 1);
  }
}

TEST(LirsCacheTest, StackPruningForgetsNonResidentHistory) {
  // Refreshing all LIR pages prunes ghost 4, so its return is HIR.
  // Each row is {key, loaded value version, cumulative misses}.
  const std::array<ExpectedAccess, 13> accesses = {{
      {1, 1, 1}, {2, 2, 2}, {3, 3, 3}, {4, 4, 4},
      {5, 5, 5}, {1, 1, 5}, {2, 2, 5}, {3, 3, 5},
      {4, 6, 6}, {6, 7, 7}, {1, 1, 7}, {2, 2, 7},
      {3, 3, 7},
  }};
  int loader_calls = 0;
  auto loader = [&](const int&) { return ++loader_calls; };
  cache::Lirs<int, int> cache(4, loader);

  for (size_t i = 0; i < accesses.size(); ++i) {
    const auto& access = accesses[i];
    SCOPED_TRACE(testing::Message() << "access=" << i + 1
                                  << ", key=" << access.key);
    ASSERT_EQ(cache.LookUpUpdate(access.key), access.value);
    ASSERT_EQ(cache.GetCacheMissCount(), access.misses);
    ASSERT_EQ(loader_calls, access.misses);
    ASSERT_EQ(cache.GetAccessCount(), i + 1);
  }
}

TEST(LirsCacheTest, HirHitOutsideStackRefreshesQueueRecency) {
  // With two HIR slots, a hit outside the stack must refresh queue recency.
  // Each row is {key, loaded value version, cumulative misses}.
  const std::array<ExpectedAccess, 15> accesses = {{
      {1, 1, 1}, {2, 2, 2}, {3, 3, 3}, {4, 4, 4},
      {5, 5, 5}, {6, 6, 6}, {7, 7, 7}, {8, 8, 8},
      {9, 9, 9}, {10, 10, 10}, {9, 9, 10}, {10, 10, 10},
      {1, 1, 10}, {11, 11, 11}, {1, 1, 11},
  }};
  int loader_calls = 0;
  auto loader = [&](const int&) { return ++loader_calls; };
  cache::Lirs<int, int> cache(10, loader);

  for (size_t i = 0; i < accesses.size(); ++i) {
    const auto& access = accesses[i];
    SCOPED_TRACE(testing::Message() << "access=" << i + 1
                                  << ", key=" << access.key);
    ASSERT_EQ(cache.LookUpUpdate(access.key), access.value);
    ASSERT_EQ(cache.GetCacheMissCount(), access.misses);
    ASSERT_EQ(loader_calls, access.misses);
    ASSERT_EQ(cache.GetAccessCount(), i + 1);
  }
}

TEST(LirsCacheTest, FiftyAccessesWithChangingWorkingSet) {
  // Mix resident promotions, ghost reloads, scans and working set changes.
  // Each row is {key, loaded value version, cumulative misses}.
  const std::array<ExpectedAccess, 50> accesses = {{
      {1, 1, 1}, {2, 2, 2}, {3, 3, 3}, {4, 4, 4},
      {4, 4, 4}, {5, 5, 5}, {4, 4, 5}, {1, 6, 6},
      {1, 6, 6}, {6, 7, 7}, {2, 8, 8}, {3, 3, 8},
      {5, 9, 9}, {5, 9, 9}, {7, 10, 10}, {8, 11, 11},
      {7, 12, 12}, {9, 13, 13}, {8, 14, 14}, {6, 15, 15},
      {10, 16, 16}, {11, 17, 17}, {12, 18, 18}, {13, 19, 19},
      {4, 20, 20}, {5, 9, 20}, {4, 20, 20}, {5, 9, 20},
      {14, 21, 21}, {15, 22, 22}, {1, 23, 23}, {2, 24, 24},
      {3, 25, 25}, {4, 20, 25}, {1, 26, 26}, {2, 27, 27},
      {3, 28, 28}, {4, 20, 28}, {1, 26, 28}, {2, 27, 28},
      {5, 29, 29}, {6, 30, 30}, {5, 31, 31}, {6, 32, 32},
      {7, 33, 33}, {8, 34, 34}, {5, 31, 34}, {6, 32, 34},
      {7, 35, 35}, {8, 36, 36},
  }};
  int loader_calls = 0;
  auto loader = [&](const int&) { return ++loader_calls; };
  cache::Lirs<int, int> cache(4, loader);

  for (size_t i = 0; i < accesses.size(); ++i) {
    const auto& access = accesses[i];
    SCOPED_TRACE(testing::Message() << "access=" << i + 1
                                  << ", key=" << access.key);
    ASSERT_EQ(cache.LookUpUpdate(access.key), access.value);
    ASSERT_EQ(cache.GetCacheMissCount(), access.misses);
    ASSERT_EQ(loader_calls, access.misses);
    ASSERT_EQ(cache.GetAccessCount(), i + 1);
  }
}

TEST(LirsCacheTest, HundredTwentyAccessesWithPromotionsAndStackPruning) {
  // Exercise repeated promotions and pruning before returning to older keys.
  // Each row is {key, loaded value version, cumulative misses}.
  const std::array<ExpectedAccess, 120> accesses = {{
      {1, 1, 1}, {2, 2, 2}, {3, 3, 3}, {4, 4, 4},
      {4, 4, 4}, {5, 5, 5}, {4, 4, 5}, {1, 6, 6},
      {1, 6, 6}, {6, 7, 7}, {2, 8, 8}, {3, 3, 8},
      {5, 9, 9}, {5, 9, 9}, {7, 10, 10}, {8, 11, 11},
      {7, 12, 12}, {9, 13, 13}, {8, 14, 14}, {6, 15, 15},
      {10, 16, 16}, {11, 17, 17}, {12, 18, 18}, {13, 19, 19},
      {4, 20, 20}, {5, 9, 20}, {4, 20, 20}, {5, 9, 20},
      {14, 21, 21}, {15, 22, 22}, {1, 23, 23}, {2, 24, 24},
      {3, 25, 25}, {4, 20, 25}, {1, 26, 26}, {2, 27, 27},
      {3, 28, 28}, {4, 20, 28}, {1, 26, 28}, {2, 27, 28},
      {5, 29, 29}, {6, 30, 30}, {5, 31, 31}, {6, 32, 32},
      {7, 33, 33}, {8, 34, 34}, {5, 31, 34}, {6, 32, 34},
      {7, 35, 35}, {8, 36, 36}, {20, 37, 37}, {21, 38, 38},
      {22, 39, 39}, {23, 40, 40}, {24, 41, 41}, {25, 42, 42},
      {26, 43, 43}, {27, 44, 44}, {28, 45, 45}, {29, 46, 46},
      {30, 47, 47}, {31, 48, 48}, {32, 49, 49}, {33, 50, 50},
      {34, 51, 51}, {35, 52, 52}, {36, 53, 53}, {37, 54, 54},
      {38, 55, 55}, {39, 56, 56}, {39, 56, 56}, {38, 57, 57},
      {37, 58, 58}, {36, 59, 59}, {35, 60, 60}, {34, 61, 61},
      {33, 62, 62}, {32, 63, 63}, {31, 64, 64}, {30, 65, 65},
      {0, 66, 66}, {-1, 67, 67}, {-2, 68, 68}, {-3, 69, 69},
      {0, 70, 70}, {-1, 71, 71}, {-2, 72, 72}, {-3, 73, 73},
      {0, 70, 73}, {-1, 71, 73}, {-2, 72, 73}, {-3, 73, 73},
      {0, 70, 73}, {-1, 71, 73}, {-2, 72, 73}, {-3, 73, 73},
      {0, 70, 73}, {-1, 71, 73}, {-2, 72, 73}, {-3, 73, 73},
      {1, 74, 74}, {1, 74, 74}, {2, 75, 75}, {2, 75, 75},
      {3, 76, 76}, {3, 76, 76}, {4, 77, 77}, {4, 77, 77},
      {5, 78, 78}, {5, 78, 78}, {6, 79, 79}, {6, 79, 79},
      {7, 80, 80}, {7, 80, 80}, {8, 81, 81}, {8, 81, 81},
      {0, 82, 82}, {-1, 83, 83}, {0, 84, 84}, {-1, 85, 85},
  }};
  int loader_calls = 0;
  auto loader = [&](const int&) { return ++loader_calls; };
  cache::Lirs<int, int> cache(4, loader);

  for (size_t i = 0; i < accesses.size(); ++i) {
    const auto& access = accesses[i];
    SCOPED_TRACE(testing::Message() << "access=" << i + 1
                                  << ", key=" << access.key);
    ASSERT_EQ(cache.LookUpUpdate(access.key), access.value);
    ASSERT_EQ(cache.GetCacheMissCount(), access.misses);
    ASSERT_EQ(loader_calls, access.misses);
    ASSERT_EQ(cache.GetAccessCount(), i + 1);
  }
}

TEST(LirsCacheTest, SixtyMixedAccessesAcrossCapacities) {
  const std::array<int, 60> keys = {
      // Reuse zero and negative keys while filling the cache.
      0, -1, 0, 1, 2, -1, 0, 3, 1, 2,
      // Interleave new pages with recently evicted pages.
      4, 5, 3, 4, 6, 5, 7, 6, 8, 7,
      // Return to a small working set and hit it repeatedly.
      0, -1, 0, -1, 1, 2, 1, 2, 3, 3,
      // Scan ten distinct pages to overflow small caches and their histories.
      10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
      // Reverse the end of the scan, then return to the original working set.
      19, 18, 17, 16, 0, -1, 0, -1, 1, 2,
      // Alternate two small sets before revisiting the oldest keys.
      3, 4, 3, 4, 5, 6, 5, 6, 0, -1,
  };
  const std::array<size_t, 6> capacities = {1, 2, 3, 4, 8, 20};
  // Independently simulated outcomes: M = load, H = resident hit.
  // Each string corresponds to the capacity at the same index above.
  const std::array<std::string, 6> expected_outcomes = {
      "MMMMMMMMMM" "MMMMMMMMMM" "MMMMMMMMMH" "MMMMMMMMMM" "HMMMMMMMMM" "MMMMMMMMMM",
      "MMHMMMHMMM" "MMMMMMMMMM" "MMMMMMMMMH" "MMMMMMMMMM" "HMMMMMMMMM" "MMMMMMMMMM",
      "MMHMMHHMMM" "MMMMMMMMMM" "MMMMMMMMMH" "MMMMMMMMMM" "HMMMMMMMMM" "MMMMMMMMMM",
      "MMHMMHHMHM" "MMMMMMMMMM" "MMMMMMMMMH" "MMMMMMMMMM" "HMMMMMMMMM" "MMMMMMMMMM",
      "MMHMMHHMHH" "MMHHMHMMMM" "MMHHMMHHMH" "MMMMMMMMMM" "HHMMMMHHMM" "MMHHMMHHHH",
      "MMHMMHHMHH" "MMHHMHMHMH" "HHHHHHHHHH" "MMMMMMMMMM" "HHHHHHHHHH" "HHHHHHHHHH",
  };

  for (size_t scenario = 0; scenario < capacities.size(); ++scenario) {
    SCOPED_TRACE(testing::Message() << "capacity=" << capacities[scenario]);
    ASSERT_EQ(expected_outcomes[scenario].size(), keys.size());
    size_t loader_calls = 0;
    size_t expected_misses = 0;
    auto loader = [&](const int& key) {
      ++loader_calls;
      return LoadPage(key);
    };
    cache::Lirs<int, int> cache(capacities[scenario], loader);

    for (size_t i = 0; i < keys.size(); ++i) {
      SCOPED_TRACE(testing::Message() << "access=" << i + 1
                                    << ", key=" << keys[i]);
      if (expected_outcomes[scenario][i] == 'M') {
        ++expected_misses;
      }
      ASSERT_EQ(cache.LookUpUpdate(keys[i]), LoadPage(keys[i]));
      ASSERT_EQ(cache.GetCacheMissCount(), expected_misses);
      ASSERT_EQ(loader_calls, expected_misses);
      ASSERT_EQ(cache.GetAccessCount(), i + 1);
    }
  }
}
