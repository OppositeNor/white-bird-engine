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
#ifndef __WBE_ALLOCATOR_FIXED_SIZE_POOL_TEST_HH__
#define __WBE_ALLOCATOR_FIXED_SIZE_POOL_TEST_HH__

#include "core/allocator/heap_allocator_fixed_size_pool.hh"
#include "global/global.hh"
#include <cstddef>
#include <gtest/gtest.h>

namespace WBE = WhiteBirdEngine;

TEST(WBEAllocFSPTest, Trait) {
    ASSERT_TRUE(WBE::AllocatorTrait<WBE::HeapAllocatorFixedSizePool>::IS_POOL);
    ASSERT_TRUE(WBE::AllocatorTrait<WBE::HeapAllocatorFixedSizePool>::IS_LIMITED_SIZE);
    ASSERT_TRUE(WBE::AllocatorTrait<WBE::HeapAllocatorFixedSizePool>::IS_GURANTEED_CONTINUOUS);
    ASSERT_TRUE(WBE::AllocatorTrait<WBE::HeapAllocatorFixedSizePool>::IS_ALLOC_FIXED_SIZE);
    ASSERT_FALSE(WBE::AllocatorTrait<WBE::HeapAllocatorFixedSizePool>::IS_ATOMIC);
}

TEST(WBEAllocFSPTest, ToString) {
    WBE::HeapAllocatorFixedSizePool pool(4, 64);

    std::string exp_1 = "{\"type\":\"HeapAllocatorFixedSizePool\",\"size\":4,\"obj_count\":0,\"max_obj\":64,\"allocated\":[]}";
    ASSERT_EQ(static_cast<std::string>(pool), exp_1);

    WBE::MemID mem_1 = pool.allocate();
    std::string exp_2 = "{\"type\":\"HeapAllocatorFixedSizePool\",\"size\":4,\"obj_count\":1,\"max_obj\":64,\"allocated\":[1]}";
    ASSERT_EQ(static_cast<std::string>(pool), exp_2);

    WBE::MemID mem_2 = pool.allocate();
    std::string exp_3 = "{\"type\":\"HeapAllocatorFixedSizePool\",\"size\":4,\"obj_count\":2,\"max_obj\":64,\"allocated\":[1,2]}";
    ASSERT_EQ(static_cast<std::string>(pool), exp_3);

    pool.deallocate(mem_1);
    std::string exp_4 = "{\"type\":\"HeapAllocatorFixedSizePool\",\"size\":4,\"obj_count\":1,\"max_obj\":64,\"allocated\":[2]}";
    ASSERT_EQ(static_cast<std::string>(pool), exp_4);

    pool.deallocate(mem_2);
    ASSERT_EQ(static_cast<std::string>(pool), exp_1);

    ASSERT_TRUE(pool.is_empty());
}

inline size_t mem_diff(void* p_mem1, void* p_mem2) {
    return reinterpret_cast<size_t>(p_mem1) - reinterpret_cast<size_t>(p_mem2);
}

template <size_t ARR_SIZE>
void test_continuous(WBE::HeapAllocatorFixedSizePool& p_pool, WBE::MemID p_mems[ARR_SIZE]) {
    // One and only one should be at the beginning
    bool found_begin = false;
    WBE::MemID begin_id = p_mems[0];
    WBE::MemID max_addr_id = p_mems[0];
    for (uint32_t i = 0; i < ARR_SIZE; ++i) {
        void* curr_addr = p_pool.get(p_mems[i]);
        if (curr_addr == p_pool.get_mem_start()) {
            if (found_begin) {
                ASSERT_TRUE(false);
            }
            found_begin = true;
            begin_id = p_mems[i];
        }
        if (curr_addr > p_pool.get(max_addr_id)) {
            max_addr_id = p_mems[i];
        } 
    }
    ASSERT_TRUE(found_begin);
    // they should be ranged within a size of ARR_SIZE * SIZE
    ASSERT_EQ(mem_diff(p_pool.get(max_addr_id), p_pool.get(begin_id)) + p_pool.get_element_size(), ARR_SIZE * p_pool.get_element_size());
    // for each memid, they should have another id which is SIZE distance apart (except the last one)
    for (uint32_t i = 0; i < ARR_SIZE; ++i) {
        if (p_mems[i] == max_addr_id) {
            continue;
        }
        bool found_next = false;
        for (uint32_t j = 0; j < ARR_SIZE; ++j) {
            if (mem_diff(p_pool.get(p_mems[j]), p_pool.get(p_mems[i])) == p_pool.get_element_size()) {
                found_next = true;
            }
        }
        ASSERT_TRUE(found_next);
    }
}

TEST(WBEAllocFSPTest, MemContinuity) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    WBE::HeapAllocatorFixedSizePool pool(16, 64);
    WBE::MemID mems[4];
    mems[0] = pool.allocate();
    mems[1] = pool.allocate();
    mems[2] = pool.allocate();
    mems[3] = pool.allocate();
    test_continuous<4>(pool, mems);
    pool.deallocate(mems[1]);
    mems[1] = mems[3];
    test_continuous<3>(pool, mems);
    for (uint32_t i = 0; i < 3; ++i) {
        pool.deallocate(mems[i]);
    }
    ASSERT_TRUE(pool.is_empty());
}


#endif
