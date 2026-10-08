#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
// #include <mutex>
#include <shared_mutex>

template <typename Key, typename Value>
class ConcurrentHashMap {
private:
    // Each shard holds an independent map and its own mutex
    struct Shard {
        mutable std::shared_mutex mtx;
        std::unordered_map<Key, Value> map;
    };

    size_t num_shards_;
    std::vector<Shard> shards_;

    // Route a key to a specific shard using hashing
    Shard& getShard(const Key& key) {
        size_t hash_val = std::hash<Key>{}(key); // brace initialization that constructs an unnamed temporary of functor "hash"
        return shards_[hash_val % num_shards_];
    }

    const Shard& getShard(const Key& key) const {
        size_t hash_val = std::hash<Key>{}(key);
        return shards_[hash_val % num_shards_];
    }

public:
    explicit ConcurrentHashMap(size_t num_shards = 16)
        : num_shards_(num_shards), shards_(num_shards) {}

    // 1. Insert or update a key-value pair
    void put(const Key& key, const Value& value) {
        Shard& shard = getShard(key);
        std::unique_lock<std::shared_mutex> lock(shard.mtx); // Locks ONLY shard's mutex
        shard.map[key] = value;
    }

    // 2. Retrieve a value by key (returns true if found)
    bool get(const Key& key, Value& value_out) const {
        const Shard& shard = getShard(key);
        std::shared_lock<std::shared_mutex> lock(shard.mtx);

        auto it = shard.map.find(key);
        if (it == shard.map.end()) {
            return false;
        }
        value_out = it->second;
        return true;
    }

    // 3. Remove a key
    bool erase(const Key& key) {
        Shard& shard = getShard(key);
        std::unique_lock<std::shared_mutex> lock(shard.mtx);
        return shard.map.erase(key) > 0;
    }
};