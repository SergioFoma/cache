#ifndef LFU_CACHE_HPP_
#define LFU_CACHE_HPP_

#include <functional>
#include <iterator>
#include <list>
#include <ostream>
#include <unordered_map>
#include <utility>

#include "cache.hpp"

namespace cache {

template <typename Key, typename Tp>
class Lfu final : public Cache<Key, Tp> {
 public:
  explicit Lfu(size_t cache_cap, loader<Key, Tp> slow_get_page)
      : Cache<Key, Tp>(std::move(slow_get_page)),
        cache_sz_(kInitSize),
        cache_cap_(std::max(cache_cap, kMinCap)) {}

  Tp LookUpUpdate(const Key& key) override {
    ++(this->access_count_);

    auto hash_it = data_base_.find(key);

    if (hash_it != data_base_.end()) {
      return MoveOldNode(key, hash_it);
    }

    ++(this->misses_count_);

    if (cache_sz_ >= cache_cap_) {
      ClearCache();
    }

    return AddNewElement(key);
  }

  // ================================== DUMP ==================================
  void Dump(std::ostream& out) const override {
    out << "\n=== LEAST FREQUENTLY USED (LFU) ===\n";

    DisplayListOfLists(out);

    DisplayDatabase(out);

    out << "===================================\n\n";
  }

 private:
  struct CacheNode {
    Key key;
    Tp val;
    size_t counter;
  };

  using CacheList = std::list<CacheNode>;
  using ElemIt = typename CacheList::iterator;

  struct FreqInfo {
    size_t freq_num;
    CacheList cache_list;

    explicit FreqInfo(size_t freq_num) : freq_num(freq_num) {}
  };

  using FreqList = std::list<FreqInfo>;
  using ListIt = typename FreqList::iterator;

  struct ElInfo {
    ElemIt elem_it;
    ListIt list_it;
  };

  static constexpr size_t kMinCounter = 1;
  static constexpr size_t kInitSize = 0;
  static constexpr size_t kMinCap = 1;

  size_t cache_sz_;
  size_t cache_cap_;

  FreqList frequencies_;
  std::unordered_map<Key, ElInfo> data_base_;

  // =============================== ALGORITHM ================================
  Tp MoveOldNode(const Key& key,
                 typename std::unordered_map<Key, ElInfo>::iterator hash_it) {
    ListIt& list_it = (hash_it->second).list_it;
    ElemIt& elem_it = (hash_it->second).elem_it;

    ListIt next_list_it = std::next(list_it);

    size_t current_freq_num = list_it->freq_num;
    size_t next_freq_num =
        (next_list_it == frequencies_.end() ? 0 : next_list_it->freq_num);

    if (next_freq_num == current_freq_num + 1) {
      return MoveNode(key, list_it, next_list_it, elem_it);
    }
    return CreateAndMove(key, list_it, next_list_it, elem_it);
  }

  Tp MoveNode(const Key& key, ListIt& list_it, ListIt& next_list_it,
              ElemIt& elem_it) {
    Tp elem_val = elem_it->val;
    ++(elem_it->counter);

    CacheList& cache_list = list_it->cache_list;
    CacheList& next_cache_list = next_list_it->cache_list;
    next_cache_list.splice(next_cache_list.end(), cache_list, elem_it);

    if (cache_list.empty()) {
      frequencies_.erase(list_it);
    }

    data_base_[key] = {elem_it, next_list_it};
    return elem_val;
  }

  Tp CreateAndMove(const Key& key, ListIt& list_it, ListIt& next_list_it,
                   ElemIt& elem_it) {
    Tp elem_val = elem_it->val;
    ++(elem_it->counter);

    auto freq_it =
        frequencies_.insert(next_list_it, FreqInfo{elem_it->counter});

    CacheList& cache_list = list_it->cache_list;
    CacheList& new_cache_list = freq_it->cache_list;
    new_cache_list.splice(new_cache_list.end(), cache_list, elem_it);

    if (cache_list.empty()) {
      frequencies_.erase(list_it);
    }

    data_base_[key] = {elem_it, freq_it};
    return elem_val;
  }

  void ClearCache() {
    CacheList& first_list = (frequencies_.begin())->cache_list;
    Key delete_key = (first_list.front()).key;

    first_list.pop_front();

    if (first_list.empty()) {
      frequencies_.erase(frequencies_.begin());
    }

    data_base_.erase(delete_key);
    --cache_sz_;
  }

  Tp AddNewElement(const Key& key) {
    Tp elem_val = this->slow_get_page_(key);
    ListIt first_list = frequencies_.begin();

    if (first_list != frequencies_.end() &&
        first_list->freq_num == kMinCounter) {
      CacheList& cache_list = first_list->cache_list;
      auto new_it = cache_list.emplace(cache_list.end(),
                                       CacheNode{key, elem_val, kMinCounter});
      data_base_[key] = {new_it, first_list};
    } else {
      CacheList new_list;
      auto new_elem_it = new_list.emplace(
          new_list.end(), CacheNode{key, elem_val, kMinCounter});
      auto freq_it =
          frequencies_.insert(frequencies_.begin(), FreqInfo{kMinCounter});
      CacheList& freq_begin = freq_it->cache_list;
      freq_begin.splice(freq_begin.end(), new_list, new_elem_it);
      data_base_[key] = {new_elem_it, freq_it};
    }

    ++cache_sz_;
    return elem_val;
  }

  // =============================== DUMP_HELP ================================
  void DisplayListOfLists(std::ostream& out) const {

    out << "Total Capacity: " << cache_cap_
        << " | Total Elements: " << cache_sz_ << '\n';

    int num = 1;
    for (auto it = frequencies_.begin(); it != frequencies_.end(); ++it) {
      out << "\nList " << num << '\n';
      DisplayList(out, it->cache_list);
      ++num;
    }
  }

  void DisplayList(std::ostream& out, const CacheList& list) const {

    int num = 1;
    for (auto it = list.begin(); it != list.end(); ++it) {
      out << "\t\t[" << num << ']' << ": Key = " << it->key
          << ", counter = " << it->counter << '\n';
      ++num;
    }
  }

  void DisplayDatabase(std::ostream& out) const {
    out << "\n--- DATA BASE ---\n";
    size_t num = 1;

    for (auto it = data_base_.begin(); it != data_base_.end(); ++it) {
      out << num << ". Key: " << it->first << '\n';
      ++num;
    }
  }

  // ==========================================================================
};
}  // namespace cache

#endif
