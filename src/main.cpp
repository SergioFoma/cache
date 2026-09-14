#include <cstdint>
#include <cstdio>
#include <exception>
#include <iostream>

#include "cache_algorithm/arc.hpp"
#include "cache_algorithm/lfu_cache.hpp"
#include "config.hpp"
#include "cache_hierarchy.hpp"

int main() {
  try {
    cache::Config config("./config/config.json");
    cache::CacheHierarchy<int, int> cache(config, [] (int key) { return key * 10; });

    // cache::Lru<int, int> cache(5, [] (int key) { return key * 10; });
    cache.LookUpUpdate(10);
    cache.LookUpUpdate(14);
    cache.LookUpUpdate(13);
    cache.LookUpUpdate(13);
    cache.LookUpUpdate(12);
    cache.LookUpUpdate(11);

    cache.Dump(std::cout);

  } catch (const std::exception& ex) {
    std::cerr << ex.what();
  }
  return 0;
}
