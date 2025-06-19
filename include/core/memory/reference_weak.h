#ifndef __WBE_REFERENCE_WEAK_H__
#define __WBE_REFERENCE_WEAK_H__

#include "reference_strong.h"
#include <concepts>

namespace WhiteBirdEngine {

template <typename T>
class RefWeak {
public:
    RefWeak()
        : control_block(nullptr) {}
    ~RefWeak() {
        deref();
    }
    template <typename T1> requires (std::convertible_to<T1*, T*>)
    RefWeak(const RefWeak<T1>& p_other) {
        WBE_DEBUG_ASSERT(p_other.control_block != nullptr);
        p_other.ref();
        control_block = reinterpret_cast<decltype(control_block)>(p_other.control_block);
    }

    template <typename T1> requires (std::convertible_to<T1*, T*>)
    RefWeak(RefWeak<T1>&& p_other) {
        WBE_DEBUG_ASSERT(p_other.control_block != nullptr);
        control_block = reinterpret_cast<decltype(control_block)>(p_other.control_block);
        p_other.control_block = nullptr;
    }

    template <typename T1> requires (std::convertible_to<T1*, T*>)
    RefWeak& operator=(const RefWeak<T1>& p_other) {
        WBE_DEBUG_ASSERT(p_other.control_block != nullptr);
        deref();
        p_other.ref();
        control_block = reinterpret_cast<decltype(control_block)>(p_other.control_block);
    }
    template <typename T1> requires (std::convertible_to<T1*, T*>)
    RefWeak& operator=(RefWeak<T1>&& p_other) {
        WBE_DEBUG_ASSERT(p_other.control_block != nullptr);
        deref();
        control_block = reinterpret_cast<decltype(control_block)>(p_other.control_block);
        p_other.control_block = nullptr;
    }

    template <typename T1> requires (std::convertible_to<T1*, T*>)
    RefWeak(Ref<T1> p_ref) {
        WBE_DEBUG_ASSERT(p_ref.control_block != nullptr);
        control_block = reinterpret_cast<decltype(control_block)>(p_ref.control_block);
        ref();
    }

    template <typename T1> requires (std::convertible_to<T1*, T*>)
    RefWeak& operator=(Ref<T1> p_ref) {
        WBE_DEBUG_ASSERT(p_ref.control_block != nullptr);
        deref();
        control_block = reinterpret_cast<decltype(control_block)>(p_ref.control_block);
        ref();
        return *this;
    }

    Ref<T> lock() {
        return Ref<T>(control_block);
    }

    Ref<const T> lock() const {
        return Ref<const T>(control_block);
    }

    bool is_valid() const {
        return control_block != nullptr && control_block->strong_ref_counter.load(std::memory_order_acquire) != 0;
    }

private:
    mutable Ref<T>::ControlBlock* control_block;

    void ref() const {
        if (control_block == nullptr) {
            return;
        }
        control_block->weak_ref_counter.fetch_add(1, std::memory_order_acq_rel);
    }

    void deref() const {
        if (control_block == nullptr) {
            return;
        }
        control_block->weak_ref_counter.fetch_sub(1, std::memory_order_acq_rel);
        uint32_t weak_ref_count = control_block->weak_ref_counter.load(std::memory_order_acquire);
        uint32_t total_count = control_block->strong_ref_counter.load(std::memory_order_acquire) + weak_ref_count;
        if (total_count == 0) {
            delete control_block;
        }
        else if (weak_ref_count) {
            control_block = nullptr;
        }
    }
};

}

#endif
