#include <cstdint>
#include <cstdio>
#include <exception>
#include <iostream>

#include "cache_algorithm/arc.hpp"
#include "cache_algorithm/belady_cache.hpp"
#include "cache_algorithm/lfu_cache.hpp"
#include "cache_hierarchy.hpp"
#include "config.hpp"

int main() {
  try {

    std::vector<int> hist = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    cache::Belady<int, int> cache(5, [](int key) { return key * 10; }, hist);

    cache.Dump(std::cout);

  } catch (const std::exception& ex) {
    std::cerr << ex.what();
  }
  return 0;
}
