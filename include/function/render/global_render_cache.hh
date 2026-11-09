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
#ifndef WBE_FILE_GLOBAL_RENDER_CACHE_HH
#define WBE_FILE_GLOBAL_RENDER_CACHE_HH

#include "core/cache/weak_cache.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/i_render_object.hh"
#include "render_defs.hh"
#include "utils/interface/i_singleton.hh"
#include "utils/utils.hh"
#include <compare>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace WhiteBirdEngine {

template <typename T>
struct TypeUUID;

/**
 * @class GlobalRenderCache
 * @brief The renderer's cache of compiled render objects, shared by every render graph. Neither copyable nor movable.
 *
 * Compilers cache the objects they compile from resources (shaders, textures, mesh buffers, pipelines) under the source
 * resource key or a key derived from it, so that every graph using the same source shares one compiled object. Entries
 * are weak: the cache never keeps an object alive, and an object is compiled again once every owner released it. Named
 * application resources do not belong here; register them on their render graph (IRenderGraph::register_resource()).
 * Confined to the rendering thread.
 */
class GlobalRenderCache final : public ISingleton<GlobalRenderCache> {
private:
    using AllocType = RenderAllocator;

public:
    /**
     * @class KeyView
     * @brief A non-owning lookup key of the cache.
     */
    struct KeyView {
        /**
         * @brief Source resource key or a key derived from it.
         */
        std::string_view resource_key;
        /**
         * @brief Runtime type id of the compiled render object.
         */
        TypeID render_object_type_id = 0;

        std::strong_ordering operator<=>(const KeyView&) const = default;
        bool operator==(const KeyView&) const = default;
    };

    /**
     * @class Key
     * @brief A cache key: one resource key can compile into several objects of different types.
     */
    struct Key {
        /**
         * @brief Source resource key or a key derived from it.
         */
        std::string resource_key;
        /**
         * @brief Runtime type id of the compiled render object.
         */
        TypeID render_object_type_id = 0;

        KeyView view() const {
            return KeyView{.resource_key = resource_key, .render_object_type_id = render_object_type_id};
        }

        friend std::strong_ordering operator<=>(const Key& p_left, const Key& p_right) {
            return p_left.view() <=> p_right.view();
        }
        friend std::strong_ordering operator<=>(const Key& p_left, const KeyView& p_right) {
            return p_left.view() <=> p_right;
        }
        friend bool operator==(const Key& p_left, const Key& p_right) {
            return p_left.view() == p_right.view();
        }
        friend bool operator==(const Key& p_left, const KeyView& p_right) {
            return p_left.view() == p_right;
        }
    };

    virtual ~GlobalRenderCache() override {
        singleton = nullptr;
    }
    GlobalRenderCache() = delete;
    GlobalRenderCache(const GlobalRenderCache&) = delete;
    GlobalRenderCache(GlobalRenderCache&&) noexcept = delete;
    GlobalRenderCache& operator=(const GlobalRenderCache&) = delete;
    GlobalRenderCache& operator=(GlobalRenderCache&&) noexcept = delete;

    /**
     * @brief Constructor.
     *
     * @param p_allocator The allocator of the cache's storage; it must outlive the cache.
     * @throws std::runtime_error If p_allocator is null.
     */
    explicit GlobalRenderCache(AllocType* p_allocator);

    /**
     * @brief Cache a compiled render object, replacing an entry of the same key and object type.
     *
     * The object is named after the key through IRenderObject::set_debug_name().
     *
     * @param p_resource_key The source resource key or a key derived from it.
     * @param p_render_object The object; its concrete type identifies the entry.
     * @throws std::runtime_error If the key is empty or the object is null.
     */
    void insert(std::string_view p_resource_key, Ref<IRenderObject> p_render_object);

    /**
     * @brief Get a live cached object by key and object type.
     *
     * @return The object, or null when none is cached.
     */
    Ref<IRenderObject> try_get(std::string_view p_resource_key, TypeID p_render_object_type_id) const;

    /**
     * @brief Get a live cached object of type T.
     *
     * @return The object, or null when none is cached.
     */
    template <typename T>
    Ref<T> try_get(std::string_view p_resource_key) const {
        return try_get(p_resource_key, TypeUUID<T>::UUID).template dynamic_cast_ref<T>();
    }

    /**
     * @brief Get a live cached object of type T, compiling and caching it when there is none.
     *
     * @param p_resource_key The source resource key or a key derived from it.
     * @param p_factory Called with no arguments on a miss; returns a non-null Ref<T>.
     * @return The cached or compiled object.
     * @throws std::runtime_error If the factory returns null, or whatever it throws.
     */
    template <typename T, typename TFactory>
    Ref<T> acquire(std::string_view p_resource_key, TFactory&& p_factory) {
        Ref<T> result = try_get<T>(p_resource_key);
        if (result != MEM_NULL) {
            return result;
        }
        result = std::forward<TFactory>(p_factory)();
        if (result == MEM_NULL) {
            throw std::runtime_error("Failed to acquire a cached render object: the factory returned null.");
        }
        insert(p_resource_key, result);
        return result;
    }

    /**
     * @brief Check whether a live object is cached under a key and object type.
     */
    bool contains(std::string_view p_resource_key, TypeID p_render_object_type_id) const;

    /**
     * @brief Remove every expired entry.
     *
     * @return The number of removed entries.
     */
    size_t prune_expired();

    /**
     * @brief Remove every entry.
     */
    void clear();

    static GlobalRenderCache* get_singleton() {
        return singleton;
    }

private:
    inline static GlobalRenderCache* singleton = nullptr;

    WeakCache<Key, IRenderObject, AllocType> cache;
};

} // namespace WhiteBirdEngine

#endif
