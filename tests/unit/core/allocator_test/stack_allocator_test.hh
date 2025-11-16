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
#ifndef __WBE_STACK_ALLOCATOR_TEST_HH__
#define __WBE_STACK_ALLOCATOR_TEST_HH__

#include "core/allocator/stack_allocator.hh"
#include <gtest/gtest.h>

namespace WBE = WhiteBirdEngine;

TEST(StackAllocator, General) {

    int val = 0;

    class TestClass {
    public:
        TestClass(int* p_val)
            : pval(p_val) {
            *p_val = 4;
            test_val = 5;
        }

        ~TestClass() {
            *pval = 2;
        }

        int test_val;
        int *pval = nullptr;
    };

    WBE::StackAllocator allocator;
    WBE::MemID test_class_mem_id = WBE::create_stack_obj<TestClass>(allocator, &val);
    ASSERT_EQ(val, 4);
    ASSERT_EQ(allocator.get_obj<TestClass>(test_class_mem_id)->test_val, 5);
    allocator.get_obj<TestClass>(test_class_mem_id)->test_val = 10;
    ASSERT_EQ(allocator.get_obj<TestClass>(test_class_mem_id)->test_val, 10);
    WBE::pop_stack_obj<TestClass>(allocator);
    ASSERT_EQ(val, 2);
}

TEST(StackAllocator, BasicAllocation) {
    WBE::StackAllocator allocator(1024);
    
    // Test initial state
    ASSERT_EQ(allocator.get_total_size(), 1024);
    ASSERT_EQ(allocator.get_alloc_size(), 0);
    
    // Test basic allocation
    WBE::MemID mem1 = allocator.allocate(64);
    ASSERT_NE(mem1, WBE::MEM_NULL);
    ASSERT_EQ(allocator.get_alloc_size(), 64);
    
    // Test getting pointer
    void* ptr1 = allocator.get(mem1);
    ASSERT_NE(ptr1, nullptr);
    
    // Test const version
    const void* const_ptr1 = static_cast<const WBE::StackAllocator&>(allocator).get(mem1);
    ASSERT_EQ(ptr1, const_ptr1);
    
    // Test writing and reading
    char* char_ptr = static_cast<char*>(ptr1);
    char_ptr[0] = 'A';
    char_ptr[63] = 'Z';
    ASSERT_EQ(char_ptr[0], 'A');
    ASSERT_EQ(char_ptr[63], 'Z');
}

TEST(StackAllocator, StackOrdering) {
    WBE::StackAllocator allocator(512);
    
    // Allocate multiple blocks
    WBE::MemID mem1 = allocator.allocate(32);
    WBE::MemID mem2 = allocator.allocate(64);
    WBE::MemID mem3 = allocator.allocate(128);
    
    ASSERT_NE(mem1, WBE::MEM_NULL);
    ASSERT_NE(mem2, WBE::MEM_NULL);
    ASSERT_NE(mem3, WBE::MEM_NULL);
    
    // Verify stack ordering (later allocations have higher addresses)
    void* ptr1 = allocator.get(mem1);
    void* ptr2 = allocator.get(mem2);
    void* ptr3 = allocator.get(mem3);
    
    ASSERT_LT(ptr1, ptr2);
    ASSERT_LT(ptr2, ptr3);
    
    // Test allocation size tracking
    ASSERT_EQ(allocator.get_alloc_size(), 32 + 64 + 128);
}

TEST(StackAllocator, PopStackFunctionality) {
    WBE::StackAllocator allocator(256);
    
    // Allocate memory
    allocator.allocate(64);
    allocator.allocate(32);
    ASSERT_EQ(allocator.get_alloc_size(), 96);
    
    // Pop 32 bytes
    void* popped = allocator.pop_stack(32);
    ASSERT_NE(popped, nullptr);
    ASSERT_EQ(allocator.get_alloc_size(), 64);
    
    // Pop 64 bytes
    popped = allocator.pop_stack(64);
    ASSERT_NE(popped, nullptr);
    ASSERT_EQ(allocator.get_alloc_size(), 0);
}

TEST(StackAllocator, ClearFunctionality) {
    WBE::StackAllocator allocator(256);
    
    // Allocate memory
    allocator.allocate(64);
    allocator.allocate(32);
    allocator.allocate(16);
    ASSERT_EQ(allocator.get_alloc_size(), 112);
    
    // Clear allocator
    allocator.clear();
    ASSERT_EQ(allocator.get_alloc_size(), 0);
    
    // Verify we can allocate after clear
    WBE::MemID new_mem = allocator.allocate(128);
    ASSERT_NE(new_mem, WBE::MEM_NULL);
    
    // Verify the pointer is valid
    void* new_ptr = allocator.get(new_mem);
    ASSERT_NE(new_ptr, nullptr);
    
    ASSERT_EQ(allocator.get_alloc_size(), 128);
}

TEST(StackAllocator, ComplexObjectLifecycle) {
    WBE::StackAllocator allocator(512);
    
    class ComplexClass {
    public:
        ComplexClass(int val, const std::string& str) : value(val), text(str) {}
        ~ComplexClass() { value = -1; }
        
        int value;
        std::string text;
    };
    
    // Create objects
    WBE::MemID obj1_mem = WBE::create_stack_obj<ComplexClass>(allocator, 42, "Hello");
    WBE::MemID obj2_mem = WBE::create_stack_obj<ComplexClass>(allocator, 99, "World");
    
    // Verify objects
    ComplexClass* obj1 = allocator.get_obj<ComplexClass>(obj1_mem);
    ComplexClass* obj2 = allocator.get_obj<ComplexClass>(obj2_mem);
    
    ASSERT_EQ(obj1->value, 42);
    ASSERT_EQ(obj1->text, "Hello");
    ASSERT_EQ(obj2->value, 99);
    ASSERT_EQ(obj2->text, "World");
    
    // Pop objects in stack order
    WBE::pop_stack_obj<ComplexClass>(allocator);
    WBE::pop_stack_obj<ComplexClass>(allocator);
}

TEST(StackAllocator, ArrayOperations) {
    WBE::StackAllocator allocator(1024);
    
    // Create array of objects
    const size_t array_size = 10;
    WBE::MemID array_start = allocator.allocate(sizeof(int) * array_size);
    
    int* array = static_cast<int*>(allocator.get(array_start));
    
    // Initialize array
    for (size_t i = 0; i < array_size; ++i) {
        array[i] = static_cast<int>(i * 10);
    }
    
    // Verify array contents
    for (size_t i = 0; i < array_size; ++i) {
        ASSERT_EQ(array[i], static_cast<int>(i * 10));
    }
    
    // Test pop_stack_obj_array (conceptually, though we need objects with destructors)
    class ArrayElement {
    public:
        ArrayElement() : value(0) {}
        ArrayElement(int val) : value(val) {}
        ~ArrayElement() { value = -1; }
        int value;
    };
    
    // Create array of objects with constructors
    WBE::MemID obj_array_start = allocator.allocate(sizeof(ArrayElement) * array_size);
    ArrayElement* obj_array = static_cast<ArrayElement*>(allocator.get(obj_array_start));
    
    // Placement new for each element
    for (size_t i = 0; i < array_size; ++i) {
        new(&obj_array[i]) ArrayElement(static_cast<int>(i));
    }
    
    // Verify construction
    for (size_t i = 0; i < array_size; ++i) {
        ASSERT_EQ(obj_array[i].value, static_cast<int>(i));
    }
    
    // Manual cleanup (simulating pop_stack_obj_array behavior)
    for (size_t i = 0; i < array_size; ++i) {
        obj_array[array_size - 1 - i].~ArrayElement();
    }
}

TEST(StackAllocator, AlignmentBehavior) {
    WBE::StackAllocator allocator(256);
    
    // Test alignment behavior with different sizes
    std::vector<size_t> test_sizes = {1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17};
    
    for (size_t size : test_sizes) {
        size_t before_alloc = allocator.get_alloc_size();
        WBE::MemID mem = allocator.allocate(size);
        size_t after_alloc = allocator.get_alloc_size();
        
        ASSERT_NE(mem, WBE::MEM_NULL);
        
        // Verify aligned size calculation
        size_t expected_aligned_size = WBE::get_align_size(size, WBE_DEFAULT_ALIGNMENT);
        size_t actual_allocated = after_alloc - before_alloc;
        ASSERT_EQ(actual_allocated, expected_aligned_size);
        
        // Verify pointer alignment
        void* ptr = allocator.get(mem);
        ASSERT_NE(ptr, nullptr);
        ASSERT_EQ(reinterpret_cast<uintptr_t>(ptr) % WBE_DEFAULT_ALIGNMENT, 0);
    }
}

TEST(StackAllocator, SizeTracking) {
    const size_t total_size = 512;
    WBE::StackAllocator allocator(total_size);
    
    // Verify initial state
    ASSERT_EQ(allocator.get_total_size(), total_size);
    ASSERT_EQ(allocator.get_alloc_size(), 0);
    
    // Allocate incrementally and verify tracking
    size_t expected_used = 0;
    for (size_t i = 1; i <= 5; ++i) {
        size_t alloc_size = i * 16;
        allocator.allocate(alloc_size);
        expected_used += WBE::get_align_size(alloc_size, WBE_DEFAULT_ALIGNMENT);
        
        ASSERT_EQ(allocator.get_alloc_size(), expected_used);
        ASSERT_EQ(allocator.get_total_size(), total_size);
    }
}

TEST(StackAllocator, EdgeCases) {
    WBE::StackAllocator allocator(64);
    
    // Test zero-size allocation
    WBE::MemID zero_mem = allocator.allocate(0);
    ASSERT_NE(zero_mem, WBE::MEM_NULL);
    
    // Verify we can get a pointer to zero-size allocation
    void* zero_ptr = allocator.get(zero_mem);
    ASSERT_NE(zero_ptr, nullptr);
    
    // Test maximum capacity usage
    size_t remaining = allocator.get_total_size() - allocator.get_alloc_size();
    WBE::MemID max_mem = allocator.allocate(remaining);
    ASSERT_NE(max_mem, WBE::MEM_NULL);
    
    // Verify we can get a pointer to max allocation
    void* max_ptr = allocator.get(max_mem);
    ASSERT_NE(max_ptr, nullptr);
    
    // Verify we used all available space
    ASSERT_GE(allocator.get_alloc_size(), allocator.get_total_size());
}

#endif
