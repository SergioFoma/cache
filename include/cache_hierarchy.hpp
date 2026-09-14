#ifndef CACHE_HIERARCHY_HPP_
#define CACHE_HIERARCHY_HPP_

#include <cassert>
#include <iomanip>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "cache.hpp"
#include "cache_algorithm/arc.hpp"
#include "cache_algorithm/lfu_cache.hpp"
#include "cache_algorithm/lirs_cache.hpp"
#include "cache_algorithm/lru_cache.hpp"
#include "cache_algorithm/two_queues.hpp"
#include "config.hpp"

namespace cache {

template <typename Key, typename Tp>
class CacheHierarchy {
 private:
  // the input layer is the last one
  std::vector<std::unique_ptr<Cache<Key, Tp>>> cache_vector_;

  std::unique_ptr<Cache<Key, Tp>> GetCacheByEnum(
      type cache_type, size_t cap, loader<Key, Tp> slow_get_page) {
    switch (cache_type) {
      case type::kLru:
        return std::make_unique<Lru<Key, Tp>>(cap, std::move(slow_get_page));
      case type::kArc:
        return std::make_unique<Arc<Key, Tp>>(cap, std::move(slow_get_page));
      case type::kTwoQueues:
        return std::make_unique<TwoQueues<Key, Tp>>(cap,
                                                    std::move(slow_get_page));
      case type::kLfu:
        return std::make_unique<Lfu<Key, Tp>>(cap, std::move(slow_get_page));
      case type::kLirs:
        return std::make_unique<Lirs<Key, Tp>>(cap, std::move(slow_get_page));
      default:
        assert(0 && "Missed cache declaration");
    }
  };

 public:
  explicit CacheHierarchy(const Config& config, loader<Key, Tp> slow_get_page) {
    const auto& cache_layers = config.GetCacheHierarchy();
    assert(!cache_layers.empty());

    auto next_loader = std::move(slow_get_page);
    for (auto it = cache_layers.crbegin(); it != cache_layers.crend(); ++it) {
      auto layer =
          GetCacheByEnum(it->first, it->second, std::move(next_loader));
      auto* next_cache = layer.get();
      cache_vector_.push_back(std::move(layer));

      next_loader = [next_cache](const Key& key) -> Tp {
        return next_cache->LookUpUpdate(key);
      };
    }
  }

  Tp LookUpUpdate(const Key& key) {
    return cache_vector_.back()->LookUpUpdate(key);
  }

  void Dump(std::ostream& out) const {
    size_t i = 0;
    for (auto layer = cache_vector_.crbegin();
         layer != cache_vector_.crend(); ++layer) {
      out << "\n=============== Cache num:" << std::setw(4)
          << i << " ==============\n";
      (*layer)->Dump(out);
      ++i;
    }
  }
};
}  // namespace cache

#endif  // CACHE_HIERARCHY_HPP_
