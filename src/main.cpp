#include <cstdint>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iostream>

#include "cache_algorithm/arc.hpp"
#include "cache_algorithm/belady_cache.hpp"
#include "cache_algorithm/lfu_cache.hpp"
#include "parser.hpp"
#include "cache_hierarchy.hpp"


int main() {
  try {
    constexpr size_t kCap = 20;

    std::vector<int> test = cache::ReadTestsData("test.txt");

    // cache::Belady<int, int> cache(kCap, [] (int key) { return key; }, test);
    cache::Arc<int, int> cache(kCap, [] (int key) { return key; });
    for (auto key : test) {
      cache.LookUpUpdate(key);
    }

    std::cout << cache.GetCacheMissCount();

  } catch (const std::exception& ex) {
    std::cerr << ex.what();
  }
  return 0;
}
