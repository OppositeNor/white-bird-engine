#ifndef __WBE_REFERENCE_WEAK_H__
#define __WBE_REFERENCE_WEAK_H__

#include "reference_strong.h"

namespace WhiteBirdEngine {

template <typename T>
class RefWeak {
public:
    RefWeak(Ref<T> p_ref)
        : control_block(p_ref.control_block) {
        ref();
    }
    ~RefWeak() {
        deref();
        uint32_t total_count = control_block->hard_ref_counter.add(control_block->weak_ref_counter.load(std::memory_order_acquire),
                                                                   std::memory_order_acquire);
        if (total_count == 0) {
            delete control_block;
        }
    }

    Ref<T> lock() {
        return Ref<T>(control_block);
    }

    Ref<const T> lock() const {
        return Ref<const T>(control_block);
    }

    bool is_valid() const {
        return control_block->hard_ref_counter.load(std::memory_order_acquire) != 0;
    }

private:
    mutable Ref<T>::ControlBlock* control_block;

    void ref() const {
        control_block->weak_ref_counter.fetch_add(1, std::memory_order_acq_rel);
    }

    void deref() const {
        control_block->weak_ref_counter.fetch_sub(1, std::memory_order_acq_rel);
    }
};

}

#endif
