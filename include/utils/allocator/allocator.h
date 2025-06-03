#ifndef __WBE_ALLOCATOR_H__
#define __WBE_ALLOCATOR_H__

#include <memory>
#include <sstream>
#include <unordered_set>
namespace WhiteBirdEngine {

class Allocator {
public:
    Allocator() = default;
    virtual ~Allocator() {}

    virtual void* allocate(size_t p_size) = 0;

    virtual void deallocate(void* p_mem) = 0;

    virtual uint32_t obj_count() const = 0;

    virtual const std::unordered_set<void*>& get_allocated() const = 0;

    virtual operator std::string() const {
        std::stringstream ss;
        ss << "Allocator{obj_count: " << obj_count() << "}";
        return ss.str();
    }
};

template <typename T, typename ...Args>
inline T* create_obj(std::shared_ptr<Allocator> p_allocator, Args&&... args) {
    T* obj = new(p_allocator->allocate(sizeof(T))) T(std::forward<Args>(args)...);
    return obj;
}

template <typename T>
inline void destroy_obj(std::shared_ptr<Allocator> p_allocator, T* p_obj) {
    p_obj->~T();
    p_allocator->deallocate(p_obj);
}

template <typename T, typename ...Args>
inline T* create_obj(Allocator& p_allocator, Args&&... args) {
    T* obj = new(p_allocator.allocate(sizeof(T))) T(std::forward<Args>(args)...);
    return obj;
}

template <typename T>
inline void destroy_obj(Allocator& p_allocator, T* p_obj) {
    p_obj->~T();
    p_allocator.deallocate(p_obj);
}


}

#endif
