#ifndef __WBE_ALLOCATOR_H__
#define __WBE_ALLOCATOR_H__

#include <memory>
#include <set>
#include <sstream>
#include <type_traits>
namespace WhiteBirdEngine {

using MemID = uint64_t;

class Allocator {
public:
    Allocator() = default;
    virtual ~Allocator() {}

    virtual MemID allocate(size_t p_size) = 0;

    virtual void deallocate(MemID p_mem) = 0;

    virtual void* get(MemID p_id) = 0;

    virtual uint32_t obj_count() const = 0;

    virtual const std::set<MemID>& get_allocated() const = 0;

    virtual operator std::string() const {
        std::stringstream ss;
        ss << "Allocator{obj_count: " << obj_count() << "}";
        return ss.str();
    }

    template <typename T>
    T* get_obj(MemID p_id) {
        return static_cast<T*>(get(p_id));
    }
};

template <typename T, typename ...Args>
inline MemID create_obj(std::shared_ptr<Allocator> p_allocator, Args&&... p_args) {
    MemID id = p_allocator->allocate(sizeof(T));
    new(p_allocator->get(id)) T(std::forward<Args>(p_args)...);
    return id;
}

template <typename T> requires std::is_destructible_v<T>
inline void destroy_obj(std::shared_ptr<Allocator> p_allocator, MemID p_id) {
    p_allocator->get_obj<T>(p_id)->~T();
    p_allocator->deallocate(p_id);
}

template <typename T> requires (!std::is_destructible_v<T>)
inline void destroy_obj(std::shared_ptr<Allocator> p_allocator, MemID p_id) {
    p_allocator->deallocate(p_id);
}

template <typename T, typename ...Args>
inline MemID create_obj(Allocator& p_allocator, Args&&... p_args) {
    MemID id = p_allocator.allocate(sizeof(T));
    new(p_allocator.get(id)) T(std::forward<Args>(p_args)...);
    return id;
}

template <typename T>
inline void destroy_obj(Allocator& p_allocator, MemID p_id) {
    p_allocator.get_obj<T>(p_id)->~T();
    p_allocator.deallocate(p_id);
}


}

#endif
