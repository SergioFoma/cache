#ifndef LIRS_HPP_
#define LIRS_HPP_

#include <list>
#include <algorithm>
#include <unordered_map>
#include <functional>
#include <iterator>
#include <ostream>
#include <string>

#include "cache.hpp"

namespace cache {

template <typename Key, typename Tp>
class Lirs final : public Cache<Key, Tp> {
public:
    explicit Lirs(size_t cache_cap, loader<Key, Tp> slow_get_page)
        : Cache<Key, Tp>(slow_get_page),
          lir_cap_(std::max<size_t>(kMinCap, cache_cap * kLirCoeff)),
          hir_cap_(std::max<size_t>(kMinCap, cache_cap * kHirCoeff)),
          cache_cap_(cache_cap) {}

    Tp LookUpUpdate(const Key& key) override{
        ++(this->access_count_);

        auto hash_it = data_base_.find(key);

        if (hash_it != data_base_.end()) {
            return EditExistingNode(key, hash_it);
        }

        return AbsoluteMiss(key);
    }

    // ================================== DUMP ==================================
    void Dump(std::ostream& out) const override {
        out << "\n=== LOW INTER-REFERENCE RECENCY SET ===\n";

        DisplayTitle(out);

        DisplayList(out, cache_, "\n--- CACHE ---\n");

        DisplayList(out, queue_, "\n--- QUEUE ---\n");

        DisplayDatabase(out);

        out << "===================================\n\n";
    }

private:
enum class NodeType {
    kLir,
    kHir,
    kNonRes
};

struct CacheNode {
    Key key;
    NodeType node_type;
    Tp val;
};

using CacheList = std::list<CacheNode>;
using CacheIt = typename CacheList::iterator;
using Queue = std::list<CacheNode>;
using QueueIt = typename Queue::iterator;


struct ElInfo {
    NodeType node_type;
    CacheIt cache_it;
    QueueIt queue_it;
};

using HashTable = std::unordered_map<Key, ElInfo>;
using HashTableIt = typename HashTable::iterator;

static constexpr size_t kMinCap = 1;
static constexpr double kLirCoeff = 0.8;
static constexpr double kHirCoeff = 0.2;

size_t lir_sz_ = 0;
size_t lir_cap_;
size_t hir_sz_ = 0;
size_t hir_cap_;
size_t cache_cap_;

CacheList cache_;
Queue queue_;
HashTable data_base_;

// =============================== ALGORITHM ================================
Tp EditExistingNode(const Key& key, HashTableIt& hash_it) {
    ElInfo& el_info = hash_it->second;;

    switch(el_info.node_type) {
        case NodeType::kLir:
            return LirHit(el_info);
        case NodeType::kHir:
            return HirHit(key, el_info);
        case NodeType::kNonRes:
            return NonResHit(key, el_info);
        default:
            break;
    }

    return {};
}

Tp LirHit(ElInfo& el_info) {
    auto cache_it = el_info.cache_it;

    cache_.splice(cache_.begin(), cache_, cache_it);

    StackPruning();

    return cache_it->val;
}

void StackPruning() {
    auto cache_it = std::prev(cache_.end());

    while (cache_it->node_type != NodeType::kLir) {
        Key current_key = cache_it->key;

        if (cache_it->node_type == NodeType::kNonRes) {
            data_base_.erase(current_key);
        } else {
            data_base_[cache_it->key].cache_it = cache_.end();
        }

        cache_it = std::prev(cache_it);

        cache_.pop_back();
    }
}

Tp HirHit(const Key& key, ElInfo& el_info) {
    bool is_in_stack = (el_info.cache_it == cache_.end() ? false : true);

    Tp elem = (el_info.queue_it)->val;
    if (!is_in_stack) {
        CacheNode new_lir = {key, NodeType::kHir, elem};
        cache_.push_front(new_lir);
        data_base_[key] = {NodeType::kHir, cache_.begin(), el_info.queue_it};

        return elem;
    }

    cache_.splice(cache_.begin(), cache_, el_info.cache_it);
    (el_info.cache_it)->node_type = NodeType::kLir;
    ++lir_sz_;

    queue_.erase(el_info.queue_it);
    --hir_sz_;

    UpdateBottomLir();

    data_base_[key] = {NodeType::kLir, cache_.begin(), queue_.end()};

    return elem;
}

void UpdateBottomLir() {

    if (lir_sz_ > lir_cap_) {
        cache_.back().node_type = NodeType::kHir;
        queue_.splice(queue_.end(), cache_, std::prev(cache_.end()));
        ++hir_sz_;
        --lir_sz_;
        StackPruning();

        Key bottom_key = queue_.back().key;
        data_base_[bottom_key] = {NodeType::kHir, cache_.end(), std::prev(queue_.end())};

        if (hir_sz_ > hir_cap_) {
           ReleaseHirList();
        }
    }
}

Tp NonResHit(const Key& key, ElInfo& el_info) {
    ++(this->misses_count_);

    Tp elem = this->slow_get_page_(key);

    *(el_info.cache_it) = {key, NodeType::kLir, elem};
    cache_.splice(cache_.begin(), cache_, el_info.cache_it);
    ++lir_sz_;

    data_base_[key] = {NodeType::kLir, cache_.begin(), queue_.end()};

    UpdateBottomLir();

    return elem;
}


Tp AbsoluteMiss(const Key& key) {
    ++(this->misses_count_);

    Tp elem = this->slow_get_page_(key);

    if (lir_sz_ < lir_cap_) {
        ++lir_sz_;
        auto new_it = cache_.emplace(cache_.begin(), CacheNode{key, NodeType::kLir, elem});
        data_base_[key] = {NodeType::kLir, new_it, queue_.end()};

        return elem;
    }

    auto cache_it = cache_.emplace(cache_.begin(), CacheNode{key, NodeType::kHir, elem});
    auto queue_it = queue_.emplace(queue_.end(), CacheNode{key, NodeType::kHir, elem});
    ++hir_sz_;

    data_base_[key] = {NodeType::kHir, cache_it, queue_it};

    if (hir_sz_ > hir_cap_) {
        ReleaseHirList();
    }

    return elem;
}

void ReleaseHirList() {
    CacheNode& node = queue_.front();
    data_base_[node.key].node_type = NodeType::kNonRes;
    data_base_[node.key].queue_it = queue_.end();
    auto hash_it = data_base_.find(node.key);

    if (hash_it != data_base_.end() && (hash_it->second).cache_it != cache_.end()) {
        ElInfo& deleted_el = hash_it->second;
        deleted_el.cache_it->node_type = NodeType::kNonRes;
    }

    queue_.pop_front();
    --hir_sz_;
}

// =============================== DUMP_HELP ================================
void DisplayTitle(std::ostream& out) const {
    out << "Cache Capacity: " << cache_cap_ << '\n';
    out  << "LIR Capacity: " <<lir_cap_ << " | LIR Size: " << lir_sz_ << '\n';
    out << "HIR Capacity: " << hir_cap_ << " | HIR Size: " << hir_sz_ << '\n';
}

void DisplayList(std::ostream& out, const std::list<CacheNode>& list, const std::string& message) const {
    out << message;

    size_t num = 1;
    for (auto it = list.begin(); it != list.end(); ++it) {
        out << num++ << ". Key: " << it->key << " | ";

        switch(it->node_type) {
            case NodeType::kLir:
                out << "[LIR]\n";
                break;
            case NodeType::kHir:
                out << "[HIR]\n";
                break;
            case NodeType::kNonRes:
                out << "[Non Res]\n";
                break;
            default:
                break;
        }
    }
}

void DisplayDatabase(std::ostream& out) const {
    out << "\n--- DATA BASE ---\n";
    size_t num = 1;

    for (auto it = data_base_.begin(); it != data_base_.end(); ++it) {
      out << num++ << ". Key: " << it->first << " --> ";

      switch((it->second).node_type) {
        case NodeType::kLir:
            out << "[LIR]\n";
            break;
        case NodeType::kHir:
            out << "[HIR]\n";
            break;
        case NodeType::kNonRes:
            out << "[Non Res]\n";
            break;
        default:
            break;
    }
  }
}

// ==========================================================================
};
} // namespace cache

#endif
