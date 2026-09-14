#ifndef CACHE_HPP_
#define CACHE_HPP_

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>

namespace cache {

template <typename Key, typename Tp>
using loader = std::function<Tp(const Key&)>;

enum class type { kLru, kArc, kTwoQueues, kLfu, kLirs };
const std::unordered_map<std::string, type> kStringToEnumTable = {
    {"lru", type::kLru},
    {"arc", type::kArc},
    {"2q", type::kTwoQueues},
    {"lfu", type::kLfu},
    {"lirs", type::kLirs}};

template <typename Key, typename Tp>
class Cache {
 protected:
  size_t misses_count_{};
  size_t access_count_{};
  loader<Key, Tp> slow_get_page_;

 public:
  explicit Cache(loader<Key, Tp> slow_get_page)
      : slow_get_page_(std::move(slow_get_page)) {};

  virtual Tp LookUpUpdate(const Key& key) = 0;
  void SwitchLoader(loader<Key, Tp> slow_get_page) {
    slow_get_page_ = std::move(slow_get_page);
  };

  virtual void Dump(std::ostream& out) const = 0;

  // getters
  size_t GetCacheMissCount() const { return misses_count_; };
  size_t GetAccessCount() const { return access_count_; };

  virtual ~Cache() = default;
};
}  // namespace cache

#endif  // CACHE_HPP_
