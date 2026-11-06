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
#ifndef WBE_FILE_MEMORY_REGRESSION_TEST_HH
#define WBE_FILE_MEMORY_REGRESSION_TEST_HH

#include "core/allocator/heap_allocator_aligned.hh"
#include "core/allocator/i_allocator.hh"
#include "core/allocator/stack_allocator.hh"
#include "core/core_utils.hh"
#include "core/memory/reference_raw.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/reference_weak.hh"
#include "core/memory/unique.hh"
#include "mock_heap_allocator_aligned.hh"
#include <atomic>
#include <cstdlib>
#include <functional>
#include <gtest/gtest.h>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace WBE = WhiteBirdEngine;

/**
 * @brief Counts how many MRDestructionCounted objects have been destroyed.
 */
inline std::atomic<int> mr_destruction_count{0};

struct MRLeft {
    virtual ~MRLeft() = default;
    int left_value = 1;
};

struct MRRight {
    virtual ~MRRight() = default;
    virtual int get_id() const {
        return 2;
    }
    int right_value = 2;
};

// MRRight is a non-primary base, so it is not at offset 0 inside MRBoth.
struct MRBoth : public MRLeft, public MRRight {
    virtual int get_id() const override {
        return 3;
    }
    int both_value = 3;
};

// A base without a virtual destructor: only the deleter captured at creation can run ~MRDerivedCounted.
struct MRPlainBase {
    int value = 4;
};

struct MRDerivedCounted : public MRPlainBase {
    MRDerivedCounted() = default;
    ~MRDerivedCounted() {
        mr_destruction_count.fetch_add(1);
    }
    MRDerivedCounted(const MRDerivedCounted&) = delete;
    MRDerivedCounted(MRDerivedCounted&&) = delete;
    MRDerivedCounted& operator=(const MRDerivedCounted&) = delete;
    MRDerivedCounted& operator=(MRDerivedCounted&&) = delete;
};

struct MRPoisoned {
    MRPoisoned() = default;
    ~MRPoisoned() {
        value.store(-1);
        mr_destruction_count.fetch_add(1);
    }
    MRPoisoned(const MRPoisoned&) = delete;
    MRPoisoned(MRPoisoned&&) = delete;
    MRPoisoned& operator=(const MRPoisoned&) = delete;
    MRPoisoned& operator=(MRPoisoned&&) = delete;
    std::atomic<int> value{7};
};

class MRSelfReferencing : public WBE::ThisRef<MRSelfReferencing> {
public:
    WBE::Ref<MRSelfReferencing> acquire_self() const {
        return acquire_this_ref();
    }
};

/**
 * @brief A thread-safe malloc-backed allocator for the multithreaded tests.
 */
class MRLockedHeapAllocator final : public WBE::HeapAllocatorAligned {
public:
    MRLockedHeapAllocator() = default;
    virtual ~MRLockedHeapAllocator() override {
        clear();
    }
    MRLockedHeapAllocator(const MRLockedHeapAllocator&) = delete;
    MRLockedHeapAllocator(MRLockedHeapAllocator&&) = delete;
    MRLockedHeapAllocator& operator=(const MRLockedHeapAllocator&) = delete;
    MRLockedHeapAllocator& operator=(MRLockedHeapAllocator&&) = delete;

    virtual WBE::MemID allocate(size_t p_size, size_t p_alignment = WBE_DEFAULT_ALIGNMENT) override {
        (void)p_alignment;
        std::scoped_lock lock(mutex);
        void* pointer = std::malloc(p_size);
        WBE::MemID id = reinterpret_cast<WBE::MemID>(pointer);
        allocations[id] = p_size;
        return id;
    }

    virtual void deallocate(WBE::MemID p_mem_id) override {
        std::scoped_lock lock(mutex);
        auto found = allocations.find(p_mem_id);
        if (found == allocations.end()) {
            ++invalid_deallocations;
            return;
        }
        std::free(reinterpret_cast<void*>(p_mem_id));
        allocations.erase(found);
    }

    virtual void* get(WBE::MemID p_mem_id) const override {
        return reinterpret_cast<void*>(p_mem_id);
    }

    virtual void clear() override {
        std::scoped_lock lock(mutex);
        for (auto& [id, size] : allocations) {
            std::free(reinterpret_cast<void*>(id));
        }
        allocations.clear();
    }

    virtual bool is_empty() const override {
        std::scoped_lock lock(mutex);
        return allocations.empty();
    }

    virtual size_t get_allocated_data_size(WBE::MemID p_mem_id) const override {
        std::scoped_lock lock(mutex);
        auto found = allocations.find(p_mem_id);
        return found == allocations.end() ? 0 : found->second;
    }

    size_t get_invalid_deallocations() const {
        std::scoped_lock lock(mutex);
        return invalid_deallocations;
    }

private:
    mutable std::mutex mutex;
    std::unordered_map<WBE::MemID, size_t> allocations;
    size_t invalid_deallocations = 0;
};

// ---------------------------------------------------------------------------------------------------------------------
// Ref
// ---------------------------------------------------------------------------------------------------------------------

TEST(WBEMemoryRegressionTest, RefUpcastToNonPrimaryBaseAddressesTheBaseSubobject) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    {
        WBE::Ref<MRBoth> both = WBE::make_ref<MRBoth>(&allocator);
        WBE::Ref<MRRight> right = both;
        ASSERT_EQ(right.get(), static_cast<MRRight*>(both.get()));
        EXPECT_EQ(right->right_value, 2);
        EXPECT_EQ(right->get_id(), 3);

        WBE::Ref<MRRight> right_assigned;
        right_assigned = both;
        ASSERT_EQ(right_assigned.get(), static_cast<MRRight*>(both.get()));

        WBE::Ref<MRBoth> both_copy = both;
        WBE::Ref<MRRight> right_moved = std::move(both_copy);
        ASSERT_EQ(right_moved.get(), static_cast<MRRight*>(both.get()));
    }
    EXPECT_TRUE(allocator.is_empty());
}

TEST(WBEMemoryRegressionTest, RefLastBaseReferenceDestroysTheCreatedType) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    mr_destruction_count.store(0);
    {
        WBE::Ref<MRPlainBase> base;
        {
            WBE::Ref<MRDerivedCounted> derived = WBE::make_ref<MRDerivedCounted>(&allocator);
            base = derived;
        }
        EXPECT_EQ(mr_destruction_count.load(), 0);
        EXPECT_EQ(base->value, 4);
    }
    EXPECT_EQ(mr_destruction_count.load(), 1);
    EXPECT_TRUE(allocator.is_empty());
}

TEST(WBEMemoryRegressionTest, RefDynamicCastRefCrossCastAddressesTheTargetSubobject) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    {
        WBE::Ref<MRLeft> left = WBE::make_ref<MRBoth>(&allocator);
        WBE::Ref<MRRight> right = left.dynamic_cast_ref<MRRight>();
        ASSERT_FALSE(right.is_null());
        MRBoth* both = dynamic_cast<MRBoth*>(left.get());
        ASSERT_NE(both, nullptr);
        ASSERT_EQ(right.get(), static_cast<MRRight*>(both));
        EXPECT_EQ(right->get_id(), 3);

        WBE::Ref<MRBoth> back = right.dynamic_cast_ref<MRBoth>();
        ASSERT_EQ(back.get(), both);
        EXPECT_EQ(back->both_value, 3);
    }
    EXPECT_TRUE(allocator.is_empty());
}

TEST(WBEMemoryRegressionTest, RefStaticMakeRefAssignsThisRef) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    {
        WBE::Ref<MRSelfReferencing> self = WBE::Ref<MRSelfReferencing>::make_ref(&allocator);
        WBE::Ref<MRSelfReferencing> acquired;
        EXPECT_NO_THROW(acquired = self->acquire_self());
        EXPECT_EQ(acquired, self);
    }
    EXPECT_TRUE(allocator.is_empty());
}

TEST(WBEMemoryRegressionTest, RefMoveAssignFromReferenceSharingTheObjectEmptiesTheSource) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    {
        WBE::Ref<int> target = WBE::make_ref<int>(&allocator, 5);
        WBE::Ref<int> source = target;
        target = std::move(source);
        EXPECT_TRUE(source.is_null()); // NOLINT
        ASSERT_FALSE(target.is_null());
        EXPECT_EQ(*target, 5);
    }
    EXPECT_TRUE(allocator.is_empty());
}

TEST(WBEMemoryRegressionTest, RefAssignNonNullMemIDOrPointerThrows) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    {
        WBE::Ref<int> ref = WBE::make_ref<int>(&allocator, 1);
        int value = 0;
        EXPECT_THROW(ref = static_cast<WBE::MemID>(1), std::runtime_error);
        EXPECT_THROW(ref = static_cast<void*>(&value), std::runtime_error);
        ASSERT_FALSE(ref.is_null());
        EXPECT_EQ(*ref, 1);
    }
    EXPECT_TRUE(allocator.is_empty());
}

TEST(WBEMemoryRegressionTest, RefGetOnControlBlockWithNullMemIDReturnsNull) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    {
        WBE::Ref<int> ref(&allocator, WBE::MEM_NULL);
        EXPECT_TRUE(ref.is_null());
        EXPECT_EQ(ref.get(), nullptr);
        WBE::Ref<MRRight> right = WBE::Ref<MRBoth>(&allocator, WBE::MEM_NULL);
        EXPECT_EQ(right.get(), nullptr);
    }
    EXPECT_TRUE(allocator.is_empty());
}

TEST(WBEMemoryRegressionTest, RefThreeWayComparisonIsConsistentWithEquality) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    {
        WBE::Ref<int> first = WBE::make_ref<int>(&allocator, 1);
        WBE::Ref<int> second = WBE::make_ref<int>(&allocator, 2);
        WBE::Ref<int> first_copy = first;
        WBE::Ref<int> null_ref;
        EXPECT_EQ(first <=> first_copy, 0);
        EXPECT_NE(first <=> second, 0);
        EXPECT_EQ((first < second), !(second < first));
        EXPECT_TRUE(null_ref < first);
        EXPECT_FALSE(first < null_ref);
    }
    EXPECT_TRUE(allocator.is_empty());
}

// ---------------------------------------------------------------------------------------------------------------------
// RefWeak
// ---------------------------------------------------------------------------------------------------------------------

TEST(WBEMemoryRegressionTest, RefWeakCopiesAndMovesEmptyReferences) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    {
        WBE::RefWeak<int> empty;
        WBE::RefWeak<int> copied(empty);
        WBE::RefWeak<int> moved(std::move(empty));
        WBE::RefWeak<int> copy_assigned;
        copy_assigned = copied;
        WBE::RefWeak<int> move_assigned;
        move_assigned = std::move(moved);
        EXPECT_TRUE(copied.is_null());
        EXPECT_TRUE(copy_assigned.is_null());
        EXPECT_TRUE(move_assigned.is_null());

        WBE::Ref<int> null_ref;
        WBE::RefWeak<int> from_null(null_ref);
        EXPECT_TRUE(from_null.is_null());
        EXPECT_TRUE(from_null.lock().is_null());

        WBE::Ref<int> strong = WBE::make_ref<int>(&allocator, 3);
        WBE::RefWeak<int> reassigned(strong);
        reassigned = null_ref;
        EXPECT_TRUE(reassigned.is_null());
    }
    EXPECT_TRUE(allocator.is_empty());
}

TEST(WBEMemoryRegressionTest, RefWeakUpcastToNonPrimaryBaseLocksTheBaseSubobject) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    {
        WBE::Ref<MRBoth> both = WBE::make_ref<MRBoth>(&allocator);
        WBE::RefWeak<MRRight> weak_right = both;
        ASSERT_EQ(weak_right.lock().get(), static_cast<MRRight*>(both.get()));

        WBE::RefWeak<MRBoth> weak_both = both;
        WBE::RefWeak<MRRight> weak_right_from_weak = weak_both;
        ASSERT_EQ(weak_right_from_weak.lock().get(), static_cast<MRRight*>(both.get()));
        EXPECT_EQ(weak_right_from_weak.lock()->get_id(), 3);
    }
    EXPECT_TRUE(allocator.is_empty());
}

TEST(WBEMemoryRegressionTest, RefWeakLockNeverResurrectsADestroyedObject) {
    constexpr int ITERATION_COUNT = 20000;
    MRLockedHeapAllocator allocator;
    mr_destruction_count.store(0);
    std::atomic<bool> lock_observed_destroyed_object{false};
    int completed_iterations = 0;
    {
        for (int i = 0; i < ITERATION_COUNT; ++i) {
            WBE::Ref<MRPoisoned> strong = WBE::make_ref<MRPoisoned>(&allocator);
            WBE::RefWeak<MRPoisoned> weak = strong;
            std::atomic<bool> start{false};
            std::thread locker([&]() {
                while (!start.load(std::memory_order_acquire)) {
                }
                for (int attempt = 0; attempt < 64; ++attempt) {
                    WBE::Ref<MRPoisoned> locked = weak.lock();
                    if (locked.is_null()) {
                        return;
                    }
                    if (locked->value.load() != 7) {
                        lock_observed_destroyed_object.store(true);
                    }
                }
            });
            start.store(true, std::memory_order_release);
            strong = nullptr;
            locker.join();
            ++completed_iterations;
            if (lock_observed_destroyed_object.load()) {
                break;
            }
        }
    }
    EXPECT_FALSE(lock_observed_destroyed_object.load());
    EXPECT_EQ(mr_destruction_count.load(), completed_iterations);
    EXPECT_EQ(allocator.get_invalid_deallocations(), 0U);
    EXPECT_TRUE(allocator.is_empty());
}

TEST(WBEMemoryRegressionTest, RefWeakWithCustomAllocatorTypeLocks) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    {
        WBE::Ref<int, WBE::MockHeapAllocatorAligned> strong = WBE::make_ref<int>(&allocator, 9);
        WBE::RefWeak<int, WBE::MockHeapAllocatorAligned> weak = strong;
        WBE::Ref<int, WBE::MockHeapAllocatorAligned> locked = weak.lock();
        ASSERT_FALSE(locked.is_null());
        EXPECT_EQ(*locked, 9);
    }
    EXPECT_TRUE(allocator.is_empty());
}

TEST(WBEMemoryRegressionTest, RefWeakRequiresValidAndNullComparison) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    {
        WBE::RefWeak<int> weak;
        EXPECT_TRUE(weak == nullptr);
        EXPECT_TRUE(weak == WBE::MEM_NULL);
        EXPECT_THROW(WBE_REQUIRES_VALID(weak), std::runtime_error);
        WBE::Ref<int> strong = WBE::make_ref<int>(&allocator, 1);
        weak = strong;
        EXPECT_FALSE(weak == nullptr);
        EXPECT_TRUE(weak != WBE::MEM_NULL);
        EXPECT_NO_THROW(WBE_REQUIRES_VALID(weak));
    }
    EXPECT_TRUE(allocator.is_empty());
}

TEST(WBEMemoryRegressionTest, RefWeakHashIsStableAcrossExpiry) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    {
        std::unordered_set<WBE::RefWeak<int>> weak_set;
        WBE::RefWeak<int> weak;
        {
            WBE::Ref<int> strong = WBE::make_ref<int>(&allocator, 1);
            weak = strong;
            weak_set.insert(weak);
            EXPECT_TRUE(weak_set.contains(weak));
        }
        // The hash must not change when the object dies, or the set can no longer find the entry.
        EXPECT_TRUE(weak_set.contains(weak));
        weak_set.erase(weak);
        EXPECT_TRUE(weak_set.empty());
    }
    EXPECT_TRUE(allocator.is_empty());
}

// ---------------------------------------------------------------------------------------------------------------------
// Unique
// ---------------------------------------------------------------------------------------------------------------------

TEST(WBEMemoryRegressionTest, UniqueUpcastToNonPrimaryBaseAddressesTheBaseSubobject) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    {
        WBE::Unique<MRBoth> both = WBE::make_unique<MRBoth>(&allocator);
        MRRight* expected = static_cast<MRRight*>(both.get());
        WBE::Unique<MRRight> right = std::move(both);
        ASSERT_EQ(right.get(), expected);
        EXPECT_EQ(right->get_id(), 3);

        WBE::Unique<MRRight> right_assigned;
        right_assigned = std::move(right);
        ASSERT_EQ(right_assigned.get(), expected);
        EXPECT_EQ((*right_assigned).right_value, 2);
    }
    EXPECT_TRUE(allocator.is_empty());
}

TEST(WBEMemoryRegressionTest, UniqueSelfMoveAssignKeepsTheObject) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    {
        WBE::Unique<int> unique = WBE::make_unique<int>(&allocator, 5);
        WBE::Unique<int>& alias = unique;
        unique = std::move(alias);
        ASSERT_FALSE(unique.is_null());
        EXPECT_EQ(*unique, 5);
    }
    EXPECT_TRUE(allocator.is_empty());
}

TEST(WBEMemoryRegressionTest, UniqueConstComparisonAndHash) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    {
        WBE::Unique<int> unique = WBE::make_unique<int>(&allocator, 5);
        const WBE::Unique<int>& const_unique = unique;
        EXPECT_FALSE(const_unique == nullptr);
        EXPECT_FALSE(const_unique == WBE::MEM_NULL);
        const std::hash<WBE::Unique<int>> hasher;
        EXPECT_NE(hasher(const_unique), WBE::MEM_NULL);
        WBE::Unique<int> empty;
        EXPECT_EQ(hasher(empty), WBE::MEM_NULL);
    }
    EXPECT_TRUE(allocator.is_empty());
}

// ---------------------------------------------------------------------------------------------------------------------
// RefRaw
// ---------------------------------------------------------------------------------------------------------------------

TEST(WBEMemoryRegressionTest, RefRawUpcastToNonPrimaryBaseAddressesTheBaseSubobject) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    {
        WBE::RefRaw<MRBoth, WBE::HeapAllocator, true> both = WBE::new_ref<MRBoth>(&allocator);
        WBE::RefRaw<MRRight, WBE::HeapAllocator, true> right = both;
        ASSERT_EQ(right.get(), static_cast<MRRight*>(both.get()));
        EXPECT_EQ(right->get_id(), 3);

        WBE::Unique<MRBoth> unique = WBE::make_unique<MRBoth>(&allocator);
        WBE::RefRes<MRRight> unique_view = unique;
        ASSERT_EQ(unique_view.get(), static_cast<MRRight*>(unique.get()));

        // Deleting through the base view destroys the whole object and frees the allocation.
        WBE::delete_ref(std::move(right));
    }
    EXPECT_TRUE(allocator.is_empty());
}

TEST(WBEMemoryRegressionTest, RefRawStackAllocation) {
    WBE::StackAllocator stack(256);
    TestObject::instance_count = 0;
    {
        WBE::RefRaw<TestObject, WBE::StackAllocator, true> first = WBE::new_ref_stack<TestObject>(&stack, 4);
        WBE::RefRaw<TestObject, WBE::StackAllocator, true> second =
            WBE::RefRaw<TestObject, WBE::StackAllocator, true>::new_ref(&stack, 5);
        EXPECT_EQ(first->value, 4);
        EXPECT_EQ(second->value, 5);
        EXPECT_EQ(TestObject::instance_count, 2);
        WBE::delete_ref_stack(std::move(second));
        WBE::delete_ref_stack(std::move(first));
        EXPECT_EQ(TestObject::instance_count, 0);
    }
}

TEST(WBEMemoryRegressionTest, RefRawHashInUnorderedSet) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    {
        WBE::RefRaw<int, WBE::HeapAllocator, true> raw = WBE::new_ref<int>(&allocator, 3);
        std::unordered_set<WBE::RefRaw<int, WBE::HeapAllocator, true>> raw_set;
        raw_set.insert(raw);
        EXPECT_TRUE(raw_set.contains(raw));
        raw_set.clear();
        WBE::delete_ref(std::move(raw));
    }
    EXPECT_TRUE(allocator.is_empty());
}

#endif
