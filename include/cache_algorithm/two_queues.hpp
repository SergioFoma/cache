#ifndef CACHING_ALGORITHMS_HPP_
#define CACHING_ALGORITHMS_HPP_

#include <algorithm>
#include <functional>
#include <list>
#include <ostream>
#include <string>
#include <unordered_map>

#include "cache.hpp"

namespace cache {

template <typename Key, typename Tp>
class TwoQueues final : public Cache<Key, Tp> {
 public:
  explicit TwoQueues(size_t cache_cap, loader<Key, Tp> slow_get_page)
      : Cache<Key, Tp>(slow_get_page),
        cache_cap_(cache_cap),
        in_cap_(std::max<size_t>(1, cache_cap * kInCoeff)),
        out_cap_(std::max<size_t>(1, cache_cap * kOutCoeff)),
        lru_cap_(std::max<size_t>(1, cache_cap * kLruCoeff)) {}

  Tp LookUpUpdate(const Key& key) override {
    ++(this->access_count_);

    auto hash_it = data_base_.find(key);
    if (hash_it == data_base_.end()) {
      return AbsoluteMiss(key);
    }

    switch (hash_it->second.cache_type) {
      case CacheType::kLru:
        return LruHit(hash_it);
      case CacheType::kOut:
        return OutHit(key, hash_it);
      case CacheType::kIn:
        return (hash_it->second).list_it->val;
      default:
        break;
    }

    return {};
  }

  void Dump(std::ostream& out) const {
    out << "\n=== TWO QUEUES CACHE DUMP ===\n";

    DisplayTitle(out);

    DisplayKeyValueCache(out, in_cache_, "\n--- IN CACHE ---\n");

    DisplayOutCache(out);

    DisplayKeyValueCache(out, lru_cache_, "\n--- LRU CACHE ---\n");

    DisplayDatabase(out);

    out << "=============================\n\n";
  }

 private:
  static constexpr double kInCoeff = 0.1;
  static constexpr double kOutCoeff = 0.3;
  static constexpr double kLruCoeff = 0.6;

  enum class CacheType { kIn, kOut, kLru };

  struct CacheNode {
    Key key;
    Tp val;
  };

  struct ElInfo {
    CacheType cache_type;
    typename std::list<CacheNode>::iterator list_it;
    typename std::list<Key>::iterator key_it;
  };

  size_t cache_cap_;
  size_t in_cap_;
  size_t out_cap_;
  size_t lru_cap_;

  std::list<CacheNode> in_cache_;
  std::list<Key> out_cache_;
  std::list<CacheNode> lru_cache_;
  std::unordered_map<Key, ElInfo> data_base_;

  Tp AbsoluteMiss(const Key& key) {
    ++(this->misses_count_);

    Tp old_elem = this->slow_get_page_(key);

    if (in_cache_.size() >= in_cap_) {
      auto node = in_cache_.back();
      in_cache_.pop_back();

      if (out_cache_.size() >= out_cap_) {
        Key out_key = out_cache_.back();
        out_cache_.pop_back();
        data_base_.erase(out_key);
      }

      auto pos = out_cache_.emplace(out_cache_.begin(), node.key);
      data_base_[node.key] = {CacheType::kOut, {}, pos};
    }

    auto pos = in_cache_.emplace(in_cache_.begin(), CacheNode{key, old_elem});
    data_base_[key] = {CacheType::kIn, pos, {}};

    return old_elem;
  }

  Tp LruHit(typename std::unordered_map<Key, ElInfo>::iterator hash_it) {
    auto list_it = hash_it->second.list_it;
    lru_cache_.splice(lru_cache_.begin(), lru_cache_, list_it);

    return lru_cache_.front().val;
  }

  Tp OutHit(const Key& key,
            typename std::unordered_map<Key, ElInfo>::iterator hash_it) {
    ++(this->misses_count_);

    Tp old_elem = this->slow_get_page_(key);
    out_cache_.erase(hash_it->second.key_it);

    if (lru_cache_.size() >= lru_cap_) {
      auto node = lru_cache_.back();
      lru_cache_.pop_back();
      data_base_.erase(node.key);
    }

    auto pos = lru_cache_.emplace(lru_cache_.begin(), CacheNode{key, old_elem});
    data_base_[key] = {CacheType::kLru, pos, {}};

    return old_elem;
  }

  void DisplayTitle(std::ostream& out) const {
    size_t in_sz = in_cache_.size();
    size_t out_sz = out_cache_.size();
    size_t lru_sz = lru_cache_.size();
    size_t total_sz = in_sz + out_sz + lru_sz;

    out << "Total Capacity: " << cache_cap_ << " | Total Elements: " << total_sz
        << '\n';

    out << "IN: " << in_sz << "/" << in_cap_ << " | "
        << "OUT: " << out_sz << "/" << out_cap_ << " | "
        << "LRU: " << lru_sz << "/" << lru_cap_ << '\n';
  }

  void DisplayKeyValueCache(std::ostream& out,
                            const std::list<CacheNode>& cache_list,
                            const std::string& message) const {
    out << message;
    size_t num = 1;

    for (auto it = cache_list.begin(); it != cache_list.end(); ++it) {
      out << num << ". Key: " << it->key << " | Val: " << it->val << '\n';
      ++num;
    }
  }

  void DisplayOutCache(std::ostream& out) const {
    out << "\n--- OUT CACHE ---\n";
    size_t num = 1;

    for (auto it = out_cache_.begin(); it != out_cache_.end(); ++it) {
      out << num << ". Key: " << *it << '\n';
      ++num;
    }
  }

  void DisplayDatabase(std::ostream& out) const {
    out << "\n--- DATA BASE ---\n";
    size_t num = 1;

    for (auto it = data_base_.begin(); it != data_base_.end(); ++it) {
      out << num << ". Key: " << it->first << " --> ";

      switch (it->second.cache_type) {
        case CacheType::kIn:
          out << "[IN]\n";
          break;
        case CacheType::kOut:
          out << "[OUT]\n";
          break;
        case CacheType::kLru:
          out << "[LRU]\n";
          break;
        default:
          break;
      }
      ++num;
    }
  }
};
} // namespace cache

#endif
