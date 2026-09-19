#ifndef BELADY_CACHE_HPP_
#define BELADY_CACHE_HPP_

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iomanip>
#include <list>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include "cache.hpp"

// NOTE: the alias of the ideal cache is Belady cache

namespace cache {

template <typename Key, typename Tp>
class Belady final : public Cache<Key, Tp> {
 public:
  explicit Belady(size_t capacity, loader<Key, Tp> slow_get_page,
                  std::vector<Key> full_cache_history)
      : Cache<Key, Tp>(std::move(slow_get_page)),
        cache_cap_(std::max<size_t>(capacity, 1)),
        full_cache_history_(std::move(full_cache_history)),
        cur_key_(full_cache_history_.begin()) {
    if (full_cache_history_.empty())
      throw std::runtime_error("Empty cache history. Non-predictable.");

    for (auto key_it = full_cache_history_.cbegin();
         key_it != full_cache_history_.cend(); ++key_it) {

      auto ht_it = el_refs_.find(*key_it);
      if (ht_it == el_refs_.end())
        el_refs_.insert(std::make_pair(*key_it, std::list<hist_iter>()));
      el_refs_[*key_it].push_back(key_it);
    }
  }

  // =============================== ALGORITHM ================================

  Tp LookUpUpdate(const Key& key) override {
    assert(storage_.size() <= cache_cap_);

    ++(this->access_count_);

    // ----- Check if user cheated =( -----

    if ((cur_key_ == full_cache_history_.cend()) || *cur_key_ != key)
      throw std::runtime_error("Mismatch key with predict.");

    auto hist_it = el_refs_.find(key);
    assert(hist_it != el_refs_.cend());
    if (hist_it->second.empty() || hist_it->second.front() != cur_key_)
      throw std::runtime_error("Mismatch key with predict.");

    // ----- Case : element was found -----

    auto ht_it = hash_map_.find(key);
    if (ht_it != hash_map_.end()) {
      hist_it->second.pop_front();
      ++cur_key_;

      return ht_it->second->second;
    }

    // ----- Case : element was not found -----

    ++(this->misses_count_);

    auto new_el = this->slow_get_page_(key);

    if (storage_.size() == cache_cap_) {
      FreeStorage();
    }

    storage_.push_front(std::make_pair(key, new_el));
    hash_map_.insert(std::make_pair(key, storage_.begin()));

    hist_it->second.pop_front();
    ++cur_key_;

    return new_el;
  }

  // ================================== DUMP ==================================

  void Dump(std::ostream& out) const override {

    out << "\n===== Belady(Ideal) cache =====\n"
        << "Cache cap:" << std::setw(9) << cache_cap_ << "|"
        << "Total elements:" << std::setw(9) << storage_.size() << "\n";
  }

 private:
  using list_iter = typename std::list<std::pair<Key, Tp>>::iterator;
  using hist_iter = typename std::vector<Key>::const_iterator;

  const size_t cache_cap_;

  std::vector<Key> full_cache_history_;
  hist_iter cur_key_;
  std::unordered_map<Key, std::list<hist_iter>> el_refs_;

  std::list<std::pair<Key, Tp>> storage_;
  std::unordered_map<Key, list_iter> hash_map_;

  void FreeStorage() {

    std::ptrdiff_t max_dst = 0;
    list_iter max_dst_el = storage_.begin();

    for (auto stored_it = storage_.begin(); stored_it != storage_.end();
         ++stored_it) {
      const auto& ref_vec = el_refs_.at(stored_it->first);
      if (ref_vec.empty()) {
        hash_map_.erase(stored_it->first);
        storage_.erase(stored_it);

        return;
      }

      const auto distance = ref_vec.front() - cur_key_;
      if (distance > max_dst) {
        max_dst = distance;
        max_dst_el = stored_it;
      }
    }
    hash_map_.erase(max_dst_el->first);
    storage_.erase(max_dst_el);
  }
};
}  // namespace cache

#endif  // BELADY_CACHE_HPP_
