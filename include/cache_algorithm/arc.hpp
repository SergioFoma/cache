#ifndef ARC_HPP_
#define ARC_HPP_

#include <algorithm>
#include <functional>
#include <list>
#include <ostream>
#include <string>
#include <unordered_map>
#include <utility>

#include "cache.hpp"

namespace cache {

template <typename Key, typename Tp>
class Arc final : public Cache<Key, Tp> {
 public:
  explicit Arc(size_t ram_cap, loader<Key, Tp> slow_get_page)
      : Cache<Key, Tp>(std::move(slow_get_page)),
        ram_cap_(std::max(ram_cap, kMinCap)),
        storage_cap_(2 * ram_cap) {}

  Tp LookUpUpdate(const Key& key) override {
    ++(this->access_count_);
    auto hash_it = data_base_.find(key);

    if (hash_it != data_base_.end()) {
      CacheType cache_type = (hash_it->second).cache_type;

      switch (cache_type) {
        case CacheType::kT1:
          return FirstTopHit(key, hash_it);
        case CacheType::kT2:
          return SecondTopHit(hash_it);
        case CacheType::kB1:
          return FirstBottomHit(key, hash_it);
        case CacheType::kB2:
          return SecondBottomHit(key, hash_it);
        default:
          break;
      }
    }

    return AbsoluteMiss(key);
  }

  // ================================== DUMP ==================================
  void Dump(std::ostream& out) const override {
    out << "\n=== ADAPTIVE REPLACEMENT CACHE (ARC) ===\n";

    DisplayTitle(out);

    DisplayKeyValueCache(out, top_1, "\n--- TOP 1 CACHE ---\n");

    DisplayKeyValueCache(out, top_2, "\n--- TOP 2 CACHE ---\n");

    DisplayKeyCache(out, bottom_1, "\n--- BOTTOM 1 CACHE ---\n");

    DisplayKeyCache(out, bottom_2, "\n--- BOTTOM 2 CACHE ---\n");

    DisplayDatabase(out);

    out << "===================================\n\n";
  }

 private:
  enum class CacheType { kT1, kB1, kT2, kB2 };

  struct CacheNode {
    Key key;
    Tp val;
  };

  using CacheList = std::list<CacheNode>;
  using ListIt = typename CacheList::iterator;
  using KeyList = std::list<Key>;
  using KeyIt = typename KeyList::iterator;

  struct ElInfo {
    CacheType cache_type;
    ListIt list_it;
    KeyIt key_it;
  };

  using HashTable = std::unordered_map<Key, ElInfo>;
  using TableIt = typename HashTable::iterator;

  static constexpr size_t kSingleStep = 1;
  static constexpr size_t kMinVal = 0;
  static constexpr size_t kMinCap = 1;
  static constexpr size_t kMinSz = 1;

  size_t ram_cap_;
  size_t storage_cap_;
  size_t adapt_param_ = 0;
  bool bottom_2_hit = true;
  bool bottom_2_miss = false;

  CacheList top_1;
  KeyList bottom_1;
  CacheList top_2;
  KeyList bottom_2;
  HashTable data_base_;

  // =============================== ALGORITHM ================================
  Tp FirstTopHit(const Key& key, TableIt& hash_it) {
    auto list_it = (hash_it->second).list_it;
    top_2.splice(top_2.begin(), top_1, list_it);
    data_base_[key] = {CacheType::kT2, list_it, {}};

    return list_it->val;
  }

  Tp SecondTopHit(TableIt& hash_it) {
    auto list_it = (hash_it->second).list_it;
    top_2.splice(top_2.begin(), top_2, list_it);

    return list_it->val;
  }

  Tp FirstBottomHit(const Key& key, TableIt& hash_it) {
    ++(this->misses_count_);

    size_t bottom_1_sz = std::max(kMinSz, bottom_1.size());
    size_t bottom_2_sz = bottom_2.size();
    size_t delta =
        (bottom_1_sz >= bottom_2_sz ? kSingleStep : bottom_2_sz / bottom_1_sz);

    adapt_param_ = std::min(ram_cap_, adapt_param_ + delta);

    Tp elem = this->slow_get_page_(key);

    bottom_1.erase(((hash_it->second).key_it));

    Replace(bottom_2_miss);

    auto top_2_it = top_2.emplace(top_2.begin(), CacheNode{key, elem});
    data_base_[key] = {CacheType::kT2, top_2_it, {}};

    return elem;
  }

  Tp SecondBottomHit(const Key& key, TableIt& hash_it) {
    ++(this->misses_count_);

    size_t bottom_1_sz = bottom_1.size();
    size_t bottom_2_sz = std::max(kMinSz, bottom_2.size());
    size_t delta =
        (bottom_2_sz >= bottom_1_sz ? kSingleStep : bottom_1_sz / bottom_2_sz);

    adapt_param_ = (adapt_param_ > delta ? adapt_param_ - delta : kMinVal);

    Tp elem = this->slow_get_page_(key);

    bottom_2.erase(((hash_it->second).key_it));

    Replace(bottom_2_hit);

    auto top_2_it = top_2.emplace(top_2.begin(), CacheNode{key, elem});
    data_base_[key] = {CacheType::kT2, top_2_it, {}};

    return elem;
  }

  Tp AbsoluteMiss(const Key& key) {
    ++(this->misses_count_);

    size_t cache_1_cap = top_1.size() + bottom_1.size();
    if (cache_1_cap >= ram_cap_) {
      if (top_1.size() < ram_cap_) {
        Key bot_key = bottom_1.back();
        bottom_1.pop_back();
        data_base_.erase(bot_key);
        Replace(bottom_2_miss);
      } else {
        CacheNode node = top_1.back();
        top_1.pop_back();
        data_base_.erase(node.key);
      }
    } else if (cache_1_cap < ram_cap_) {
      size_t total_cap = cache_1_cap + top_2.size() + bottom_2.size();
      if (total_cap == storage_cap_) {
        Key bot_key = bottom_2.back();
        bottom_2.pop_back();
        data_base_.erase(bot_key);
        --total_cap;
      }
      if (top_1.size() + top_2.size() >= ram_cap_) {
        Replace(bottom_2_miss);
      }
    }

    return LoadNewElem(key);
  }

  Tp LoadNewElem(const Key& key) {
    Tp elem = this->slow_get_page_(key);
    auto top_1_it = top_1.emplace(top_1.begin(), CacheNode{key, elem});
    data_base_[key] = {CacheType::kT1, top_1_it, {}};
    return elem;
  }

  void Replace(bool bottom_2_hit) {
    bool clear_top_1 =
        !top_1.empty() && (top_1.size() > adapt_param_ ||
                           (top_1.size() == adapt_param_ && bottom_2_hit));

    if (top_2.empty() || clear_top_1) {
      if (top_1.empty())
        return;

      CacheNode elem = top_1.back();
      top_1.pop_back();
      auto bot_1_hit = bottom_1.emplace(bottom_1.begin(), elem.key);
      data_base_[elem.key] = {CacheType::kB1, {}, bot_1_hit};
    } else {
      CacheNode elem = top_2.back();
      top_2.pop_back();
      auto bot_2_it = bottom_2.emplace(bottom_2.begin(), elem.key);
      data_base_[elem.key] = {CacheType::kB2, {}, bot_2_it};
    }
  }

  // =============================== DUMP_HELP ================================
  void DisplayTitle(std::ostream& out) const {
    size_t top_1_sz = top_1.size();
    size_t top_2_sz = top_2.size();
    size_t bottom_1_sz = bottom_1.size();
    size_t bottom_2_sz = bottom_2.size();
    size_t storage_sz = top_1_sz + top_2_sz + bottom_1_sz + bottom_2_sz;

    out << "Cache Capacity: " << ram_cap_
        << " | Total Capacity: " << storage_cap_
        << " | Total Elements: " << storage_sz << '\n';

    out << "TOP 1: " << top_1_sz << "/" << adapt_param_ << " | "
        << "BOTTOM 1: " << bottom_1_sz << '\n'
        << "TOP 2: " << top_2_sz << "/" << ram_cap_ - adapt_param_ << " | "
        << "BOTTOM 2: " << bottom_2_sz << '\n';
  }

  void DisplayKeyCache(std::ostream& out, const KeyList& list,
                       const std::string& message) const {
    out << message;

    size_t num = 1;
    for (auto iterator = list.begin(); iterator != list.end(); ++iterator) {
      out << num << ". KEY: " << *iterator << '\n';
      ++num;
    }
  }

  void DisplayKeyValueCache(std::ostream& out, const CacheList& list,
                            const std::string& message) const {
    out << message;

    size_t num = 1;
    for (auto iterator = list.begin(); iterator != list.end(); ++iterator) {
      out << num << ". KEY: " << iterator->key << " | Val: " << iterator->val
          << '\n';
      ++num;
    }
  }

  void DisplayDatabase(std::ostream& out) const {
    out << "\n--- DATA BASE ---\n";
    size_t num = 1;

    for (auto it = data_base_.begin(); it != data_base_.end(); ++it) {
      out << num << ". Key: " << it->first << " --> ";

      switch (it->second.cache_type) {
        case CacheType::kT1:
          out << "[T1]\n";
          break;
        case CacheType::kT2:
          out << "[T2]\n";
          break;
        case CacheType::kB1:
          out << "[B1]\n";
          break;
        case CacheType::kB2:
          out << "[B2]\n";
          break;
        default:
          break;
      }
      ++num;
    }
  }

  // ==========================================================================
};
}  // namespace cache

#endif
