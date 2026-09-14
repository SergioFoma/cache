#ifndef LRU_CACHE_HPP_
#define LRU_CACHE_HPP_

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <iomanip>
#include <list>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include "cache.hpp"

namespace cache {

template <typename Key, typename Tp>
class Lru final : public Cache<Key, Tp> {
 public:
  explicit Lru(size_t cap, loader<Key, Tp> slow_get_page)
      : Cache<Key, Tp>(std::move(slow_get_page)),
        cache_cap_(std::max<size_t>(cap, 1)) {};

  // =============================== ALGORITHM ================================
  Tp LookUpUpdate(const Key& key) override {
    assert(storage_.size() <= cache_cap_);

    (this->access_count_)++;

    auto ht_it = hash_map_.find(key);
    if (ht_it != hash_map_.end()) {
      auto list_it = ht_it->second;
      storage_.splice(storage_.begin(), storage_, list_it);

      return list_it->second;
    }

    (this->misses_count_)++;

    Tp element_copy = this->slow_get_page_(key);

    if (storage_.size() == cache_cap_) {
      hash_map_.erase(storage_.back().first);
      storage_.pop_back();
    }

    storage_.push_front(std::make_pair(key, element_copy));
    hash_map_.insert(std::make_pair(key, storage_.begin()));

    return element_copy;
  }

  // ================================== DUMP ==================================

  void Dump(std::ostream& out) const override {

    DisplayTitle(out);
    DisplayStorage(out);

    out << "============================================\n\n";
  }

 private:
  using list_iter = typename std::list<std::pair<Key, Tp>>::iterator;

  const size_t cache_cap_;
  std::list<std::pair<Key, Tp>> storage_;
  std::unordered_map<Key, list_iter> hash_map_;

  // =============================== DUMP_HELP ================================

  void DisplayTitle(std::ostream& out) const {
    out << "\n=== Last Recently Used cache (LRU cache) ===\n" 
    << "Cache cap:" << std::setw(9) << cache_cap_<< "|"
    << "Total elements:" << std::setw(9) <<storage_.size() << "\n";
  }

  void DisplayStorage(std::ostream& out) const {
    size_t i = 0;
    out << "\n----------------- Storage ------------------\n";
    for (const auto& el : storage_) {
      out << "No:" << std::setw(4) << i << "|"
          << "Key:" << std::setw(12) << el.first << "|"
          << "Value:" << std::setw(12) << el.second << "\n";
      ++i;
    }
  }

  // ==========================================================================
};
}  // namespace cache

#endif  // LRU_CACHE_HPP
