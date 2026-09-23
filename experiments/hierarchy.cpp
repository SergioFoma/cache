#include <iostream>
#include <stdexcept>
#include "cache_hierarchy.hpp"
int main(int argc, char** argv) {
  try {
    if (argc != 2) throw std::runtime_error("Expected JSON configuration path");
    cache::ConfigJson config(argv[1]);
    size_t capacity, count;
    while (std::cin >> capacity >> count) {
      size_t loads = 0;
      cache::CacheHierarchy<int,int> hierarchy(config, [&](const int& key) { ++loads; return key; });
      for (size_t i = 0; i < count; ++i) {
        int key;
        if (!(std::cin >> key)) throw std::runtime_error("Incomplete trace");
        if (hierarchy.LookUpUpdate(key) != key) throw std::runtime_error("Wrong value");
      }
      if (hierarchy.GetCacheMissCount() != loads || hierarchy.GetAccessCount() != count)
        throw std::runtime_error("Hierarchy counter mismatch");
      std::cout << hierarchy.GetCacheHitCount() << std::endl;
    }
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
