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
    RefWeak(const RefWeak<T>& p_other) {
        WBE_DEBUG_ASSERT(p_other.control_block != nullptr);
        p_other.ref();
        control_block = p_other.control_block;
    }
    RefWeak(RefWeak<T>&& p_other) {
        WBE_DEBUG_ASSERT(p_other.control_block != nullptr);
        control_block = p_other.control_block;
        p_other.control_block = nullptr;
    }
    RefWeak& operator=(const RefWeak<T>& p_other) {
        WBE_DEBUG_ASSERT(p_other.control_block != nullptr);
        if (*this == p_other) {
            return *this;
        }
        deref();
        p_other.ref();
        control_block = p_other.control_block;
        return *this;
    }
    RefWeak& operator=(RefWeak<T>&& p_other) {
        WBE_DEBUG_ASSERT(p_other.control_block != nullptr);
        if (*this == p_other) {
            return *this;
        }
        deref();
        control_block = p_other.control_block;
        p_other.control_block = nullptr;
        return *this;
    }
    RefWeak(Ref<T> p_ref) {
        WBE_DEBUG_ASSERT(p_ref.control_block != nullptr);
        control_block = p_ref.control_block;
        ref();
    }
    RefWeak& operator=(Ref<T> p_ref) {
        WBE_DEBUG_ASSERT(p_ref.control_block != nullptr);
        deref();
        control_block = p_ref.control_block;
        ref();
        return *this;
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
    RefWeak(Ref<T1> p_ref) {
        WBE_DEBUG_ASSERT(p_ref.control_block != nullptr);
        control_block = reinterpret_cast<decltype(control_block)>(p_ref.control_block);
        ref();
    }

    template <typename T1> requires (std::convertible_to<T1*, T*>)
    RefWeak& operator=(const RefWeak<T1>& p_other) {
        WBE_DEBUG_ASSERT(p_other.control_block != nullptr);
        if (*this == p_other) {
            return *this;
        }
        deref();
        p_other.ref();
        control_block = reinterpret_cast<decltype(control_block)>(p_other.control_block);
        return *this;
    }
    template <typename T1> requires (std::convertible_to<T1*, T*>)
    RefWeak& operator=(RefWeak<T1>&& p_other) {
        WBE_DEBUG_ASSERT(p_other.control_block != nullptr);
        if (*this == p_other) {
            return *this;
        }
        deref();
        control_block = reinterpret_cast<decltype(control_block)>(p_other.control_block);
        p_other.control_block = nullptr;
        return *this;
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

    template <typename T1>
    bool operator==(const RefWeak<T1>& p_other) const {
        return control_block == reinterpret_cast<decltype(control_block)>(p_other.control_block);
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
        uint32_t weak_ref_count = control_block->weak_ref_counter.fetch_sub(1, std::memory_order_acq_rel) - 1;
        uint32_t total_count = control_block->strong_ref_counter.load(std::memory_order_acquire) + weak_ref_count;
        if (weak_ref_count == 0 && total_count == 0) {
            delete control_block;
        }
        control_block = nullptr;
    }
};

}

#endif
