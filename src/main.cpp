#include <cstdint>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iostream>

#include "cache_algorithm/arc.hpp"
#include "cache_algorithm/belady_cache.hpp"
#include "cache_algorithm/lfu_cache.hpp"
#include "cache_algorithm/lru_cache.hpp"
#include "cache_algorithm/two_queues.hpp"
#include "cache_hierarchy.hpp"
#include "parser.hpp"

int main() {
  try {
    constexpr size_t kCap = 150;

    std::vector<int> test = cache::ReadTestsData("tests/test.txt");
    cache::Config config("config/config.json");

    cache::CacheHierarchy<int, int> cache_hierarchy(
        config, [](int key) { return key; });
    cache::Belady<int, int> cache_bel(kCap, [](int key) { return key; }, test);
    cache::TwoQueues<int, int> cache_2q(kCap, [](int key) { return key; });
    cache::Lru<int, int> cache_lru(kCap, [](int key) { return key; });
    cache::Lfu<int, int> cache_lfu(kCap, [](int key) { return key; });
    cache::Arc<int, int> cache_arc(kCap, [](int key) { return key; });
    cache::Lirs<int, int> cache_lirs(kCap, [](int key) { return key; });

    for (auto key : test) {
      cache_hierarchy.LookUpUpdate(key);
      cache_bel.LookUpUpdate(key);
      cache_2q.LookUpUpdate(key);
      cache_lru.LookUpUpdate(key);
      cache_lfu.LookUpUpdate(key);
      cache_arc.LookUpUpdate(key);
      cache_lirs.LookUpUpdate(key);
    }

    std::cout << "Cache Hierarchy: " << cache_hierarchy.GetCacheMissCount() << "\n";
    std::cout << "Belady Cache: " << cache_bel.GetCacheMissCount() << '\n';
    std::cout << "2Q: " << cache_2q.GetCacheMissCount() << '\n';
    std::cout << "LRU: " << cache_lru.GetCacheMissCount() << '\n';
    std::cout << "LFU: " << cache_lfu.GetCacheMissCount() << '\n';
    std::cout << "ARC: " << cache_arc.GetCacheMissCount() << '\n';
    std::cout << "LIRS: " << cache_lirs.GetCacheMissCount() << '\n';

  } catch (const std::exception& ex) {
    std::cerr << ex.what();
  }
  return 0;
}
