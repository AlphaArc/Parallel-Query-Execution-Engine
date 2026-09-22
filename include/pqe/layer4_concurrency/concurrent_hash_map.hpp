#ifndef PQE_CONCURRENCY_CONCURRENT_HASH_MAP_HPP
#define PQE_CONCURRENCY_CONCURRENT_HASH_MAP_HPP

#include "spin_lock.hpp"
#include <unordered_map>
#include <vector>
#include <functional>
#include <shared_mutex>

namespace pqe::concurrency {

    /**
     * @brief A highly optimized Concurrent Sharded Hash Map designed for multi-core Group By aggregations.
     *        It splits the key space into independent shards, each protected by an ultra-fast SpinLock.
     *        This eliminates the need for thread-local map merging and reduces contention dramatically.
     */
    template <typename Key, typename Value, std::size_t NumShards = 256>
    class ConcurrentShardedMap {
    private:
        struct alignas(64) Shard {
            SpinLock lock;
            std::unordered_map<Key, Value> map;
        };

        std::vector<Shard> shards_;
        std::hash<Key> hasher_;

        [[nodiscard]] inline std::size_t get_shard_index(const Key& key) const noexcept {
            return hasher_(key) % NumShards;
        }

    public:
        ConcurrentShardedMap() : shards_(NumShards) {}

        /**
         * @brief Atomically applies an update function to the value associated with the key.
         *        If the key does not exist, it default constructs the value before applying the update.
         */
        template <typename UpdateFunc>
        inline void update(const Key& key, UpdateFunc&& updater) {
            std::size_t shard_idx = get_shard_index(key);
            Shard& shard = shards_[shard_idx];

            ScopedSpinLock lock(shard.lock);
            updater(shard.map[key]);
        }
        
        /**
         * @brief Gathers all key-value pairs from all shards into a single vector.
         *        This is typically called at the end of the query (single-threaded).
         */
        [[nodiscard]] std::vector<std::pair<Key, Value>> gather_all() const {
            std::vector<std::pair<Key, Value>> result;
            
            // Optional: reserve capacity if we pre-calculate total size
            std::size_t total_elements = 0;
            for (const auto& shard : shards_) {
                total_elements += shard.map.size();
            }
            result.reserve(total_elements);

            for (const auto& shard : shards_) {
                // Since this is called post-parallel execution, we don't strictly need to lock
                // if we guarantee no writers exist. But locking ensures absolute safety.
                // We use const_cast here just for the lock (as it mutates the atomic flag).
                ScopedSpinLock lock(const_cast<SpinLock&>(shard.lock));
                for (const auto& [k, v] : shard.map) {
                    result.emplace_back(k, v);
                }
            }
            
            return result;
        }
    };

} // namespace pqe::concurrency

#endif // PQE_CONCURRENCY_CONCURRENT_HASH_MAP_HPP
