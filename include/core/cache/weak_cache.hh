/* Copyright 2025 OppositeNor

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#ifndef WBE_FILE_WEAK_CACHE_HH
#define WBE_FILE_WEAK_CACHE_HH

#include "core/allocator/heap_allocator.hh"
#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/reference_weak.hh"
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace WhiteBirdEngine {

/**
 * @class WeakCache
 * @brief A keyed cache of weakly held objects. Movable, not copyable.
 *
 * The cache never keeps an object alive: an entry expires when the last strong reference to its object is released, and
 * lookups treat an expired entry as missing. Expired entries keep their map slot until they are replaced, erased, or
 * pruned with prune_expired(). Keys are ordered with std::less<>, so a lookup may use any type comparable with TKey.
 * The cache is not synchronized; its owner confines it to one thread or synchronizes access externally.
 *
 * @tparam TKey The key type.
 * @tparam TValue The cached object type.
 * @tparam AllocType The allocator type of the cache's storage.
 * @tparam RefAllocType The allocator type of the cached references.
 */
template <typename TKey, typename TValue, typename AllocType, typename RefAllocType = HeapAllocator>
class WeakCache final {
public:
    WeakCache() = delete;
    ~WeakCache() = default;
    WeakCache(const WeakCache&) = delete;
    WeakCache& operator=(const WeakCache&) = delete;
    /**
     * @brief Move constructor; the moved-from cache is empty.
     */
    WeakCache(WeakCache&& p_other) noexcept : entries(std::move(p_other.entries)) {
        p_other.entries.clear();
    }
    /**
     * @brief Move assignment; the moved-from cache is empty.
     */
    WeakCache& operator=(WeakCache&& p_other) noexcept {
        if (this != &p_other) {
            entries = std::move(p_other.entries);
            p_other.entries.clear();
        }
        return *this;
    }

    /**
     * @brief Construct an empty cache.
     *
     * @param p_allocator The allocator of the cache's storage; it must outlive the cache.
     * @throws std::runtime_error If p_allocator is null.
     */
    explicit WeakCache(AllocType* p_allocator) : entries(validate_allocator(p_allocator)) {
    }

    /**
     * @brief Cache an object under a key, replacing any earlier entry.
     *
     * @param p_key The key.
     * @param p_value The object; the cache holds it weakly.
     * @throws std::runtime_error If p_value is null.
     */
    void insert(TKey p_key, const Ref<TValue, RefAllocType>& p_value) {
        if (p_value == MEM_NULL) {
            throw std::runtime_error("Failed to insert into the weak cache: the value is null.");
        }
        entries.insert_or_assign(std::move(p_key), RefWeak<TValue, RefAllocType>(p_value));
    }

    /**
     * @brief Get the live object cached under a key.
     *
     * @param p_key The key, or a value comparable with it.
     * @return The object, or null when no live object is cached under the key.
     */
    template <typename TLookup>
    Ref<TValue, RefAllocType> try_get(const TLookup& p_key) const {
        auto entry = entries.find(p_key);
        if (entry == entries.end()) {
            return MEM_NULL;
        }
        RefWeak<TValue, RefAllocType> weak = entry->second;
        if (weak.is_null()) {
            return MEM_NULL;
        }
        return weak.lock();
    }

    /**
     * @brief Get the live object cached under a key.
     *
     * @param p_key The key, or a value comparable with it.
     * @return The object.
     * @throws std::runtime_error If no live object is cached under the key.
     */
    template <typename TLookup>
    Ref<TValue, RefAllocType> get(const TLookup& p_key) const {
        Ref<TValue, RefAllocType> result = try_get(p_key);
        if (result == MEM_NULL) {
            throw std::runtime_error("Failed to get from the weak cache: no live object is cached under the key.");
        }
        return result;
    }

    /**
     * @brief Get the live object cached under a key, creating and caching it when there is none.
     *
     * @param p_key The key.
     * @param p_factory Called with no arguments when no live object is cached; returns the object to cache.
     * @return The cached or created object.
     * @throws std::runtime_error If p_factory returns null, or whatever p_factory throws.
     */
    template <typename TFactory>
    Ref<TValue, RefAllocType> acquire(TKey p_key, TFactory&& p_factory) {
        Ref<TValue, RefAllocType> result = try_get(p_key);
        if (result != MEM_NULL) {
            return result;
        }
        result = std::forward<TFactory>(p_factory)();
        if (result == MEM_NULL) {
            throw std::runtime_error("Failed to acquire from the weak cache: the factory returned null.");
        }
        insert(std::move(p_key), result);
        return result;
    }

    /**
     * @brief Check whether a live object is cached under a key.
     */
    template <typename TLookup>
    bool contains(const TLookup& p_key) const {
        auto entry = entries.find(p_key);
        return entry != entries.end() && !entry->second.is_null();
    }

    /**
     * @brief Remove the entry under a key, if any.
     *
     * @return True when an entry, live or expired, was removed.
     */
    template <typename TLookup>
    bool erase(const TLookup& p_key) {
        auto entry = entries.find(p_key);
        if (entry == entries.end()) {
            return false;
        }
        entries.erase(entry);
        return true;
    }

    /**
     * @brief Remove every expired entry.
     *
     * @return The number of removed entries.
     */
    size_t prune_expired() {
        size_t removed = 0;
        for (auto entry = entries.begin(); entry != entries.end();) {
            if (entry->second.is_null()) {
                entry = entries.erase(entry);
                ++removed;
            } else {
                ++entry;
            }
        }
        return removed;
    }

    /**
     * @brief Remove every entry.
     */
    void clear() {
        entries.clear();
    }

    /**
     * @brief Get the number of entries, including expired ones not yet pruned.
     */
    size_t size() const {
        return entries.size();
    }

private:
    Map<TKey, RefWeak<TValue, RefAllocType>, AllocType> entries;

    static AllocType* validate_allocator(AllocType* p_allocator) {
        if (p_allocator == nullptr) {
            throw std::runtime_error("Failed to create weak cache: the allocator is null.");
        }
        return p_allocator;
    }
};

} // namespace WhiteBirdEngine

#endif
