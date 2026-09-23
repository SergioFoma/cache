// Batch protocol: capacity, request count, then integer keys; one CSV row per trace.
#include <iostream>
#include <memory>
#include <vector>
#include <stdexcept>
#include "cache_algorithm/lru_cache.hpp"
#include "cache_algorithm/lfu_cache.hpp"
#include "cache_algorithm/arc.hpp"
#include "cache_algorithm/two_queues.hpp"
#include "cache_algorithm/lirs_cache.hpp"
#include "cache_algorithm/belady_cache.hpp"

int main() {
  try {
    size_t capacity, count;
    while (std::cin >> capacity >> count) {
      if (capacity < 10 || count == 0) throw std::runtime_error("Require capacity >= 10 and nonempty trace");
      std::vector<int> trace(count);
      for (int& key : trace) if (!(std::cin >> key)) throw std::runtime_error("Incomplete trace");
      size_t loads = 0;
      cache::loader<int,int> loader = [&](const int& key) { ++loads; return key; };
      std::vector<std::unique_ptr<cache::Cache<int,int>>> caches;
      caches.push_back(std::make_unique<cache::Lru<int,int>>(capacity, loader));
      caches.push_back(std::make_unique<cache::Lfu<int,int>>(capacity, loader));
      caches.push_back(std::make_unique<cache::Arc<int,int>>(capacity, loader));
      caches.push_back(std::make_unique<cache::TwoQueues<int,int>>(capacity, loader));
      caches.push_back(std::make_unique<cache::Lirs<int,int>>(capacity, loader));
      caches.push_back(std::make_unique<cache::Belady<int,int>>(capacity, loader, trace));
      for (size_t i = 0; i < caches.size(); ++i) {
        loads = 0;
        for (int key : trace) if (caches[i]->LookUpUpdate(key) != key) throw std::runtime_error("Wrong value");
        if (loads != caches[i]->GetCacheMissCount() || caches[i]->GetAccessCount() != count)
          throw std::runtime_error("Counter mismatch");
        if (i) std::cout << ',';
        std::cout << caches[i]->GetCacheHitCount();
      }
      std::cout << std::endl;
    }
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
