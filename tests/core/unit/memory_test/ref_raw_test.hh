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
#ifndef __WBE_REF_RAW_TEST_HH__
#define __WBE_REF_RAW_TEST_HH__

#include "core/memory/reference_raw.hh"
#include "mock_heap_allocator_aligned.hh"
#include <gtest/gtest.h>
#include <stdexcept>

namespace WBE = WhiteBirdEngine;

struct TestObject {
    int value;
    inline static int instance_count = 0;
    
    TestObject(int p_v) : value(p_v) {
        instance_count++;
    }
    ~TestObject() {
        instance_count--;
        value = -1; // Mark as destroyed
    }
};

class DerivedTestObject : public TestObject {
public:
    DerivedTestObject(int p_v = 0) : TestObject(p_v) {}
};

TEST(WBERefRawTest, DefaultConstruction) {
    // Test default construction creates empty reference
    WBE::RefRaw<TestObject> ref;
    
    ASSERT_EQ(ref.get(), nullptr);
    ASSERT_TRUE(ref == nullptr);
    ASSERT_TRUE(ref == WBE::MEM_NULL);
    ASSERT_TRUE(ref == static_cast<void*>(nullptr));
}

TEST(WBERefRawTest, StaticNewRefCreation) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    // Test static new_ref method
    auto ref = WBE::RefRaw<TestObject>::new_ref(&allocator, 42);
    
    ASSERT_NE(ref.get(), nullptr);
    ASSERT_EQ(ref->value, 42);
    ASSERT_EQ(TestObject::instance_count, 1);
    ASSERT_FALSE(ref == nullptr);
    
    // Manual cleanup
    WBE::RefRaw<TestObject>::delete_ref(std::move(ref));
    ASSERT_EQ(TestObject::instance_count, 0);
}

TEST(WBERefRawTest, GlobalNewRefCreation) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    // Test global new_ref function
    auto ref = WBE::new_ref<TestObject>(&allocator, 123);
    
    ASSERT_NE(ref.get(), nullptr);
    ASSERT_EQ(ref->value, 123);
    ASSERT_EQ(TestObject::instance_count, 1);
    
    // Manual cleanup
    WBE::delete_ref(std::move(ref));
    ASSERT_EQ(TestObject::instance_count, 0);
}

TEST(WBERefRawTest, NullAllocatorThrows) {
    TestObject::instance_count = 0;
    
    // Test that null allocator throws exception
    ASSERT_THROW(WBE::RefRaw<TestObject>::new_ref(nullptr, 42), std::runtime_error);
    ASSERT_THROW((WBE::new_ref<TestObject>(static_cast<WBE::MockHeapAllocatorAligned*>(nullptr), 42)), std::runtime_error);
    
    ASSERT_EQ(TestObject::instance_count, 0);
}

TEST(WBERefRawTest, CopyConstructor) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    auto ref1 = WBE::new_ref<TestObject>(&allocator, 99);
    WBE::RefRaw<TestObject, WBE::MockHeapAllocatorAligned> ref2(ref1); // Copy constructor
    
    ASSERT_EQ(ref1.get(), ref2.get());
    ASSERT_EQ(ref1->value, ref2->value);
    ASSERT_EQ(ref2->value, 99);
    ASSERT_EQ(TestObject::instance_count, 1); // Still only one object
    
    // Both references point to same object
    ref1->value = 77;
    ASSERT_EQ(ref2->value, 77);
    
    // Manual cleanup through one reference
    WBE::delete_ref(std::move(ref1));
    ASSERT_EQ(TestObject::instance_count, 0);
    
    // Note: ref2 is now dangling but this is expected behavior for raw references
}

TEST(WBERefRawTest, MoveConstructor) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    auto ref1 = WBE::new_ref<TestObject>(&allocator, 88);
    TestObject* original_ptr = ref1.get();
    
    WBE::RefRaw<TestObject, WBE::MockHeapAllocatorAligned> ref2(std::move(ref1)); // Move constructor
    
    ASSERT_EQ(ref1.get(), nullptr); // ref1 is now empty
    ASSERT_EQ(ref2.get(), original_ptr); // ref2 owns the object
    ASSERT_EQ(ref2->value, 88);
    ASSERT_EQ(TestObject::instance_count, 1);
    
    WBE::delete_ref(std::move(ref2));
    ASSERT_EQ(TestObject::instance_count, 0);
}

TEST(WBERefRawTest, CopyAssignment) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    auto ref1 = WBE::new_ref<TestObject>(&allocator, 55);
    WBE::RefRaw<TestObject, WBE::MockHeapAllocatorAligned> ref2;
    
    ASSERT_EQ(ref2.get(), nullptr);
    
    ref2 = ref1; // Copy assignment
    
    ASSERT_EQ(ref1.get(), ref2.get());
    ASSERT_EQ(ref2->value, 55);
    ASSERT_EQ(TestObject::instance_count, 1);
    
    WBE::delete_ref(std::move(ref1));
    ASSERT_EQ(TestObject::instance_count, 0);
}

TEST(WBERefRawTest, MoveAssignment) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    auto ref1 = WBE::new_ref<TestObject>(&allocator, 66);
    TestObject* original_ptr = ref1.get();
    WBE::RefRaw<TestObject, WBE::MockHeapAllocatorAligned> ref2;
    
    ref2 = std::move(ref1); // Move assignment
    
    ASSERT_EQ(ref1.get(), nullptr);
    ASSERT_EQ(ref2.get(), original_ptr);
    ASSERT_EQ(ref2->value, 66);
    ASSERT_EQ(TestObject::instance_count, 1);
    
    WBE::delete_ref(std::move(ref2));
    ASSERT_EQ(TestObject::instance_count, 0);
}

TEST(WBERefRawTest, ArrowOperator) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    auto ref = WBE::new_ref<TestObject>(&allocator, 100);
    
    // Test non-const arrow operator
    ASSERT_EQ(ref->value, 100);
    ref->value = 200;
    ASSERT_EQ(ref->value, 200);
    
    // Test const arrow operator
    const auto& const_ref = ref;
    ASSERT_EQ(const_ref->value, 200);
    
    WBE::delete_ref(std::move(ref));
    ASSERT_EQ(TestObject::instance_count, 0);
}

TEST(WBERefRawTest, DereferenceOperator) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    auto ref = WBE::new_ref<TestObject>(&allocator, 300);
    
    // Test non-const dereference operator
    ASSERT_EQ((*ref).value, 300);
    (*ref).value = 400;
    ASSERT_EQ((*ref).value, 400);
    
    // Test const dereference operator
    const auto& const_ref = ref;
    ASSERT_EQ((*const_ref).value, 400);
    
    WBE::delete_ref(std::move(ref));
    ASSERT_EQ(TestObject::instance_count, 0);
}

TEST(WBERefRawTest, GetMethod) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    WBE::RefRaw<TestObject> empty_ref;
    ASSERT_EQ(empty_ref.get(), nullptr);
    
    auto ref = WBE::new_ref<TestObject>(&allocator, 500);
    TestObject* ptr = ref.get();
    ASSERT_NE(ptr, nullptr);
    ASSERT_EQ(ptr->value, 500);
    
    // Test const get method
    const auto& const_ref = ref;
    const TestObject* const_ptr = const_ref.get();
    ASSERT_EQ(const_ptr, ptr);
    ASSERT_EQ(const_ptr->value, 500);
    
    WBE::delete_ref(std::move(ref));
    ASSERT_EQ(TestObject::instance_count, 0);
}

TEST(WBERefRawTest, EqualityOperators) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    WBE::RefRaw<TestObject> empty_ref1;
    WBE::RefRaw<TestObject> empty_ref2;
    auto ref1 = WBE::new_ref<TestObject>(&allocator, 111);
    auto ref2 = WBE::new_ref<TestObject>(&allocator, 222);
    auto ref1_copy = ref1; // Copy of ref1
    
    // Test equality between empty references
    ASSERT_TRUE(empty_ref1 == empty_ref2);
    ASSERT_FALSE(empty_ref1 != empty_ref2);
    
    // Test equality with nullptr
    ASSERT_TRUE(empty_ref1 == nullptr);
    ASSERT_FALSE(ref1 == nullptr);
    
    // Test equality with MEM_NULL
    ASSERT_TRUE(empty_ref1 == WBE::MEM_NULL);
    ASSERT_FALSE(ref1 == WBE::MEM_NULL);
    
    // Test equality with void* nullptr
    ASSERT_TRUE(empty_ref1 == static_cast<void*>(nullptr));
    ASSERT_FALSE(ref1 == static_cast<void*>(nullptr));
    
    // Test equality between different objects
    ASSERT_FALSE(ref1 == ref2);
    ASSERT_TRUE(ref1 != ref2);
    
    // Test equality with copy
    ASSERT_TRUE(ref1 == ref1_copy);
    ASSERT_FALSE(ref1 != ref1_copy);
    
    WBE::delete_ref(std::move(ref1));
    WBE::delete_ref(std::move(ref2));
    ASSERT_EQ(TestObject::instance_count, 0);
}

TEST(WBERefRawTest, ComparisonExceptions) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    WBE::RefRaw<TestObject> ref;
    
    // Test exception for non-null pointer comparison
    void* non_null_ptr = reinterpret_cast<void*>(0x1234);
    ASSERT_THROW(bool _ = ref == non_null_ptr, std::runtime_error);
    
    // Test exception for non-null MemID comparison
    WBE::MemID non_null_id(42);
    ASSERT_THROW(bool _ = ref == non_null_id, std::runtime_error);
    
    ASSERT_EQ(TestObject::instance_count, 0);
}

// Helper classes for template conversion testing
struct BaseClass {
    int base_value;
    BaseClass(int p_v) : base_value(p_v) {}
    virtual ~BaseClass() = default;
};

struct DerivedClass : public BaseClass {
    int derived_value;
    DerivedClass(int p_b, int p_d) : BaseClass(p_b), derived_value(p_d) {}
};

TEST(WBERefRawTest, TemplateConversionConstructor) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    
    // Create RefRaw<DerivedClass>
    auto derived_ref = WBE::new_ref<DerivedClass>(&allocator, 10, 20);
    ASSERT_EQ(derived_ref->base_value, 10);
    ASSERT_EQ(derived_ref->derived_value, 20);
    
    // Convert to RefRaw<BaseClass> using template copy constructor
    WBE::RefRaw<BaseClass> base_ref(derived_ref);
    
    ASSERT_EQ(base_ref->base_value, 10);
    ASSERT_EQ(derived_ref->base_value, 10); // Original still valid
    
    // Both should point to the same object
    derived_ref->base_value = 30;
    ASSERT_EQ(base_ref->base_value, 30);
    
    WBE::delete_ref(std::move(derived_ref));
}

TEST(WBERefRawTest, TemplateConversionMoveConstructor) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    
    auto derived_ref = WBE::new_ref<DerivedClass>(&allocator, 40, 50);
    DerivedClass* original_ptr = derived_ref.get();
    
    // Move convert to RefRaw<BaseClass>
    WBE::RefRaw<BaseClass> base_ref(std::move(derived_ref));
    
    ASSERT_EQ(derived_ref.get(), nullptr); // Moved from
    ASSERT_EQ(base_ref.get(), reinterpret_cast<BaseClass*>(original_ptr));
    ASSERT_EQ(base_ref->base_value, 40);
    
    WBE::delete_ref(std::move(base_ref));
}

TEST(WBERefRawTest, TemplateConversionAssignment) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    
    auto derived_ref = WBE::new_ref<DerivedClass>(&allocator, 60, 70);
    WBE::RefRaw<BaseClass> base_ref;
    
    ASSERT_EQ(base_ref.get(), nullptr);
    
    // Copy assign derived to base
    base_ref = derived_ref;
    
    ASSERT_NE(base_ref.get(), nullptr);
    ASSERT_EQ(base_ref->base_value, 60);
    ASSERT_EQ(derived_ref->base_value, 60); // Both valid
    
    WBE::delete_ref(std::move(derived_ref));
}

TEST(WBERefRawTest, TemplateConversionMoveAssignment) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    
    auto derived_ref = WBE::new_ref<DerivedClass>(&allocator, 80, 90);
    WBE::RefRaw<BaseClass> base_ref;
    
    // Move assign derived to base
    base_ref = std::move(derived_ref);
    
    ASSERT_EQ(derived_ref.get(), nullptr); // Moved from
    ASSERT_NE(base_ref.get(), nullptr);
    ASSERT_EQ(base_ref->base_value, 80);
    
    WBE::delete_ref(std::move(base_ref));
}

TEST(WBERefRawTest, ManualResourceManagement) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    allocator.clear_call_log();
    
    // Create reference
    auto ref = WBE::new_ref<TestObject>(&allocator, 999);
    ASSERT_EQ(TestObject::instance_count, 1);
    
    // Verify allocation happened
    std::string log = allocator.get_call_log();
    ASSERT_NE(log.find("allocate"), std::string::npos);
    
    allocator.clear_call_log();
    
    // Manual deletion
    WBE::delete_ref(std::move(ref));
    ASSERT_EQ(TestObject::instance_count, 0);
    ASSERT_EQ(ref.get(), nullptr); // Reference should be nulled
    
    // Verify deallocation happened
    log = allocator.get_call_log();
    ASSERT_NE(log.find("deallocate"), std::string::npos);
}

TEST(WBERefRawTest, MultipleReferencesToSameObject) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    auto ref1 = WBE::new_ref<TestObject>(&allocator, 777);
    auto ref2 = ref1; // Copy - both point to same object
    auto ref3 = ref1; // Another copy
    
    ASSERT_EQ(TestObject::instance_count, 1); // Only one object
    ASSERT_EQ(ref1.get(), ref2.get());
    ASSERT_EQ(ref1.get(), ref3.get());
    
    // Modifying through any reference affects all
    ref2->value = 888;
    ASSERT_EQ(ref1->value, 888);
    ASSERT_EQ(ref3->value, 888);
    
    // Delete through one reference - others become dangling (expected behavior)
    WBE::delete_ref(std::move(ref1));
    ASSERT_EQ(TestObject::instance_count, 0);
    
    // ref2 and ref3 are now dangling - this is the manual management aspect
}

TEST(WBERefRawTest, NumFieldBasicConstructor) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    // Test basic constructor sets num to 1
    auto ref = WBE::new_ref<TestObject>(&allocator, 42);
    ASSERT_EQ(ref.get_num(), 1);
    
    WBE::delete_ref(std::move(ref));
    ASSERT_EQ(TestObject::instance_count, 0);
}

TEST(WBERefRawTest, NumFieldCustomConstructor) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    // Create array of 5 objects
    WBE::MemID array_id = WBE::create_obj_array_align<TestObject>(allocator, 5, 100);
    WBE::RefRaw<TestObject> array_ref(array_id, 5, &allocator);
    
    ASSERT_EQ(array_ref.get_num(), 5);
    ASSERT_NE(array_ref.get(), nullptr);
    ASSERT_EQ(array_ref->value, 100); // First element value
    
    WBE::delete_ref(std::move(array_ref));
}

TEST(WBERefRawTest, NumFieldZeroThrowsException) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    WBE::MemID mem_id = WBE::create_obj_align<TestObject>(allocator, 42);
    
    // Test that num = 0 throws exception
    ASSERT_THROW({
        WBE::RefRaw<TestObject> ref(mem_id, 0, &allocator);
    }, std::runtime_error);
    
    // Clean up the allocated memory manually
    WBE::destroy_obj<TestObject>(allocator, mem_id);
    ASSERT_EQ(TestObject::instance_count, 0);
}

TEST(WBERefRawTest, NumFieldCopyPreservation) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    // Create array reference with num = 3
    WBE::MemID array_id = WBE::create_obj_array_align<TestObject>(allocator, 3, 200);
    WBE::RefRaw<TestObject> original(array_id, 3, &allocator);
    ASSERT_EQ(original.get_num(), 3);
    
    // Copy constructor should preserve num
    WBE::RefRaw<TestObject> copy_constructed(original);
    ASSERT_EQ(copy_constructed.get_num(), 3);
    
    // Copy assignment should preserve num
    WBE::RefRaw<TestObject> copy_assigned;
    copy_assigned = original;
    ASSERT_EQ(copy_assigned.get_num(), 3);
    
    WBE::delete_ref(std::move(original));
}

TEST(WBERefRawTest, NumFieldMovePreservation) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    // Create array reference with num = 4
    WBE::MemID array_id = WBE::create_obj_array_align<TestObject>(allocator, 4, 300);
    WBE::RefRaw<TestObject> original(array_id, 4, &allocator);
    ASSERT_EQ(original.get_num(), 4);
    
    // Move constructor should preserve num
    WBE::RefRaw<TestObject> move_constructed(std::move(original));
    ASSERT_EQ(move_constructed.get_num(), 4);
    ASSERT_EQ(original.get(), nullptr); // Original should be moved from
    
    // Create another reference for move assignment test
    WBE::MemID array_id2 = WBE::create_obj_array_align<TestObject>(allocator, 6, 400);
    WBE::RefRaw<TestObject> move_source(array_id2, 6, &allocator);
    ASSERT_EQ(move_source.get_num(), 6);
    
    // Move assignment should preserve num
    WBE::RefRaw<TestObject> move_assigned;
    move_assigned = std::move(move_source);
    ASSERT_EQ(move_assigned.get_num(), 6);
    ASSERT_EQ(move_source.get(), nullptr); // Source should be moved from
    
    WBE::delete_ref(std::move(move_constructed));
    WBE::delete_ref(std::move(move_assigned));
}

TEST(WBERefRawTest, NumFieldTemplateConversions) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    
    // Create array of DerivedClass with num = 7
    WBE::MemID array_id = WBE::create_obj_array_align<DerivedClass>(allocator, 7, 500, 600);
    WBE::RefRaw<DerivedClass> derived_ref(array_id, 7, &allocator);
    ASSERT_EQ(derived_ref.get_num(), 7);
    
    // Template copy constructor should preserve num
    WBE::RefRaw<BaseClass> base_copy(derived_ref);
    ASSERT_EQ(base_copy.get_num(), 7);
    ASSERT_EQ(derived_ref.get_num(), 7); // Original unchanged
    
    // Template move constructor should preserve num
    WBE::MemID array_id2 = WBE::create_obj_array_align<DerivedClass>(allocator, 8, 700, 800);
    WBE::RefRaw<DerivedClass> derived_move_source(array_id2, 8, &allocator);
    WBE::RefRaw<BaseClass> base_moved(std::move(derived_move_source));
    ASSERT_EQ(base_moved.get_num(), 8);
    ASSERT_EQ(derived_move_source.get(), nullptr); // Moved from
    
    // Template copy assignment should preserve num
    WBE::RefRaw<BaseClass> base_copy_assigned;
    base_copy_assigned = derived_ref;
    ASSERT_EQ(base_copy_assigned.get_num(), 7);
    
    // Template move assignment should preserve num
    WBE::MemID array_id3 = WBE::create_obj_array_align<DerivedClass>(allocator, 9, 900, 1000);
    WBE::RefRaw<DerivedClass> derived_move_assign_source(array_id3, 9, &allocator);
    WBE::RefRaw<BaseClass> base_move_assigned;
    base_move_assigned = std::move(derived_move_assign_source);
    ASSERT_EQ(base_move_assigned.get_num(), 9);
    ASSERT_EQ(derived_move_assign_source.get(), nullptr); // Moved from
    
    WBE::delete_ref(std::move(derived_ref));
    WBE::delete_ref(std::move(base_moved));
    WBE::delete_ref(std::move(base_move_assigned));
}

TEST(WBERefRawTest, NumFieldVariousSizes) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    // Test various array sizes and verify num field
    for (size_t i = 1; i <= 10; ++i) {
        WBE::MemID array_id = WBE::create_obj_array_align<TestObject>(allocator, i, static_cast<int>(i * 10));
        WBE::RefRaw<TestObject> ref(array_id, i, &allocator);
        
        ASSERT_EQ(ref.get_num(), i);
        ASSERT_NE(ref.get(), nullptr);
        ASSERT_EQ(ref->value, static_cast<int>(i * 10)); // First element
        
        WBE::delete_ref(std::move(ref));
    }
    
    ASSERT_EQ(TestObject::instance_count, 0);
}

TEST(WBERefRawTest, GetNumMethodConst) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    // Test get_num() on single object
    auto single_ref = WBE::new_ref<TestObject>(&allocator, 123);
    ASSERT_EQ(single_ref.get_num(), 1);
    
    // Test const access to get_num()
    const auto& const_single_ref = single_ref;
    ASSERT_EQ(const_single_ref.get_num(), 1);
    
    WBE::delete_ref(std::move(single_ref));
    
    // Test get_num() on array
    WBE::MemID array_id = WBE::create_obj_array_align<TestObject>(allocator, 12, 456);
    WBE::RefRaw<TestObject> array_ref(array_id, 12, &allocator);
    ASSERT_EQ(array_ref.get_num(), 12);
    
    // Test const access to get_num() on array
    const auto& const_array_ref = array_ref;
    ASSERT_EQ(const_array_ref.get_num(), 12);
    
    WBE::delete_ref(std::move(array_ref));
    ASSERT_EQ(TestObject::instance_count, 0);
}

TEST(WBERefRawTest, NumFieldDeleteArrayHandling) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    // Create array with multiple elements
    WBE::MemID array_id = WBE::create_obj_array_align<TestObject>(allocator, 5, 777);
    WBE::RefRaw<TestObject> array_ref(array_id, 5, &allocator);
    
    ASSERT_EQ(array_ref.get_num(), 5);
    ASSERT_EQ(TestObject::instance_count, 5); // 5 objects created
    
    // Delete should handle array correctly based on num field
    WBE::delete_ref(std::move(array_ref));
    ASSERT_EQ(TestObject::instance_count, 0); // All objects destroyed
}

TEST(WBERefRawTest, NumFieldComplexScenario) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    // Create original array reference
    WBE::MemID array_id = WBE::create_obj_array_align<TestObject>(allocator, 4, 999);
    WBE::RefRaw<TestObject> original(array_id, 4, &allocator);
    ASSERT_EQ(original.get_num(), 4);
    ASSERT_EQ(TestObject::instance_count, 4);
    
    // Create multiple copies - they should all preserve num = 4
    WBE::RefRaw<TestObject> copy1(original);
    WBE::RefRaw<TestObject> copy2 = original;
    WBE::RefRaw<TestObject> copy3;
    copy3 = original;
    
    ASSERT_EQ(copy1.get_num(), 4);
    ASSERT_EQ(copy2.get_num(), 4);
    ASSERT_EQ(copy3.get_num(), 4);
    ASSERT_EQ(TestObject::instance_count, 4); // Still only 4 objects
    
    // All references point to same array
    ASSERT_EQ(original.get(), copy1.get());
    ASSERT_EQ(original.get(), copy2.get());
    ASSERT_EQ(original.get(), copy3.get());
    
    // Move operations should also preserve num
    WBE::RefRaw<TestObject> moved1(std::move(copy1));
    ASSERT_EQ(moved1.get_num(), 4);
    ASSERT_EQ(copy1.get(), nullptr); // Moved from
    
    WBE::RefRaw<TestObject> moved2;
    moved2 = std::move(copy2);
    ASSERT_EQ(moved2.get_num(), 4);
    ASSERT_EQ(copy2.get(), nullptr); // Moved from
    
    // Delete through any reference deletes the entire array
    WBE::delete_ref(std::move(original));
    ASSERT_EQ(TestObject::instance_count, 0);
    
    // Other references are now dangling (expected behavior for raw references)
}

TEST(WBERefRawTest, GetMethodWithIndex) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    // Test get() with default index (0) on single object
    auto single_ref = WBE::new_ref<TestObject>(&allocator, 100);
    
    // Test basic get() method (should work same as before)
    ASSERT_NE(single_ref.get(), nullptr);
    ASSERT_EQ(single_ref.get()->value, 100);
    
    // Test get() with index 0 (should be same as get())
    ASSERT_NE(single_ref.get(0), nullptr);
    ASSERT_EQ(single_ref.get(0)->value, 100);
    ASSERT_EQ(single_ref.get(), single_ref.get(0));
    
    // Test that index 1 throws exception for single object (num = 1)
    ASSERT_THROW(single_ref.get(1), std::runtime_error);
    ASSERT_THROW(single_ref.get(5), std::runtime_error);
    
    WBE::delete_ref(std::move(single_ref));
    ASSERT_EQ(TestObject::instance_count, 0);
}

TEST(WBERefRawTest, GetMethodWithNullAllocator) {
    WBE::RefRaw<TestObject> empty_ref;
    
    // Test that get() returns nullptr for empty reference (all indices)
    ASSERT_EQ(empty_ref.get(), nullptr);
    ASSERT_EQ(empty_ref.get(0), nullptr);
    ASSERT_EQ(empty_ref.get(1), nullptr);  // Should not throw for null allocator
    
    // Test const version
    const auto& const_empty_ref = empty_ref;
    ASSERT_EQ(const_empty_ref.get(), nullptr);
    ASSERT_EQ(const_empty_ref.get(0), nullptr);
    ASSERT_EQ(const_empty_ref.get(1), nullptr);
}

TEST(WBERefRawTest, OperatorBracket) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    // Test operator[] on single object
    auto single_ref = WBE::new_ref<TestObject>(&allocator, 200);
    ASSERT_EQ(single_ref[0].value, 200);
    
    // Test modifying through operator[]
    single_ref[0].value = 300;
    ASSERT_EQ(single_ref[0].value, 300);
    
    // Test out of bounds on single object (num = 1)
    ASSERT_THROW(single_ref[1], std::runtime_error);
    ASSERT_THROW(single_ref[5], std::runtime_error);
    
    WBE::delete_ref(std::move(single_ref));
    ASSERT_EQ(TestObject::instance_count, 0);
}

TEST(WBERefRawTest, OperatorBracketWithNullAllocator) {
    WBE::RefRaw<TestObject> empty_ref;
    
    // Test that operator[] throws exception for null allocator
    ASSERT_THROW(empty_ref[0], std::runtime_error);
    ASSERT_THROW(empty_ref[1], std::runtime_error);
    
    // Test const version
    const auto& const_empty_ref = empty_ref;
    ASSERT_THROW(const_empty_ref[0], std::runtime_error);
    ASSERT_THROW(const_empty_ref[1], std::runtime_error);
}

TEST(WBERefRawTest, GetAndOperatorBracketEquivalence) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    // Test with single object
    auto single_ref = WBE::new_ref<TestObject>(&allocator, 555);
    
    // Verify get(0) and operator[0] point to same object
    ASSERT_EQ(single_ref.get(0), &single_ref[0]);
    ASSERT_EQ(single_ref.get(0)->value, single_ref[0].value);
    ASSERT_EQ(single_ref[0].value, 555);
    
    // Test const version
    const auto& const_single_ref = single_ref;
    ASSERT_EQ(const_single_ref.get(0), &const_single_ref[0]);
    ASSERT_EQ(const_single_ref.get(0)->value, const_single_ref[0].value);
    
    WBE::delete_ref(std::move(single_ref));
    ASSERT_EQ(TestObject::instance_count, 0);
}

TEST(WBERefRawTest, GetMethodWithIndexArray) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    // Test get() with various indices on array
    WBE::MemID array_id = create_obj_array_align<TestObject>(allocator, 5, 0);
    WBE::RefRaw<TestObject> array_ref(array_id, 5, &allocator);
    
    // Set different values for each array element
    for (size_t i = 0; i < 5; ++i) {
        array_ref.get(i)->value = static_cast<int>(i * 10);
    }
    
    // Test accessing each element
    for (size_t i = 0; i < 5; ++i) {
        ASSERT_NE(array_ref.get(i), nullptr);
        ASSERT_EQ(array_ref.get(i)->value, static_cast<int>(i * 10));
    }
    
    // Test out of bounds access
    ASSERT_THROW(array_ref.get(5), std::runtime_error);
    ASSERT_THROW(array_ref.get(10), std::runtime_error);
    
    // Manual cleanup
    WBE::destroy_obj_array_align<TestObject>(allocator, array_id, 5);
    ASSERT_EQ(TestObject::instance_count, 0);
}

TEST(WBERefRawTest, GetMethodWithIndexConst) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    // Create array and set values
    WBE::MemID array_id = create_obj_array_align<TestObject>(allocator, 4, 0);
    WBE::RefRaw<TestObject> array_ref(array_id, 4, &allocator);
    
    for (size_t i = 0; i < 4; ++i) {
        array_ref.get(i)->value = static_cast<int>(i * 100);
    }
    
    // Test const access
    const auto& const_array_ref = array_ref;
    for (size_t i = 0; i < 4; ++i) {
        const TestObject* const_ptr = const_array_ref.get(i);
        ASSERT_NE(const_ptr, nullptr);
        ASSERT_EQ(const_ptr->value, static_cast<int>(i * 100));
    }
    
    // Test const out of bounds access
    ASSERT_THROW(const_array_ref.get(4), std::runtime_error);
    
    // Manual cleanup
    WBE::destroy_obj_array_align<TestObject>(allocator, array_id, 4);
    ASSERT_EQ(TestObject::instance_count, 0);
}

TEST(WBERefRawTest, OperatorBracketArray) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    // Test operator[] on array
    WBE::MemID array_id = create_obj_array_align<TestObject>(allocator, 6, 0);
    WBE::RefRaw<TestObject> array_ref(array_id, 6, &allocator);
    
    // Set values using operator[]
    for (size_t i = 0; i < 6; ++i) {
        array_ref[i].value = static_cast<int>(i * 50);
    }
    
    // Verify values using operator[]
    for (size_t i = 0; i < 6; ++i) {
        ASSERT_EQ(array_ref[i].value, static_cast<int>(i * 50));
    }
    
    // Test out of bounds access
    ASSERT_THROW(array_ref[6], std::runtime_error);
    ASSERT_THROW(array_ref[100], std::runtime_error);
    
    // Manual cleanup
    WBE::destroy_obj_array_align<TestObject>(allocator, array_id, 6);
    ASSERT_EQ(TestObject::instance_count, 0);
}

TEST(WBERefRawTest, OperatorBracketConstArray) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    // Create array and set values
    WBE::MemID array_id = create_obj_array_align<TestObject>(allocator, 3, 0);
    WBE::RefRaw<TestObject> array_ref(array_id, 3, &allocator);
    
    array_ref[0].value = 111;
    array_ref[1].value = 222;
    array_ref[2].value = 333;
    
    // Test const operator[]
    const auto& const_array_ref = array_ref;
    ASSERT_EQ(const_array_ref[0].value, 111);
    ASSERT_EQ(const_array_ref[1].value, 222);
    ASSERT_EQ(const_array_ref[2].value, 333);
    
    // Test const out of bounds access
    ASSERT_THROW(const_array_ref[3], std::runtime_error);
    
    // Manual cleanup
    WBE::destroy_obj_array_align<TestObject>(allocator, array_id, 3);
    ASSERT_EQ(TestObject::instance_count, 0);
}

TEST(WBERefRawTest, GetAndOperatorBracketEquivalenceArray) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    // Create array
    WBE::MemID array_id = create_obj_array_align<TestObject>(allocator, 4, 0);
    WBE::RefRaw<TestObject> array_ref(array_id, 4, &allocator);
    
    // Set values and verify get() and operator[] return same references
    for (size_t i = 0; i < 4; ++i) {
        array_ref[i].value = static_cast<int>(i * 25);
        
        // Verify get() and operator[] point to same object
        ASSERT_EQ(array_ref.get(i), &array_ref[i]);
        ASSERT_EQ(array_ref.get(i)->value, array_ref[i].value);
    }
    
    // Test const version
    const auto& const_array_ref = array_ref;
    for (size_t i = 0; i < 4; ++i) {
        ASSERT_EQ(const_array_ref.get(i), &const_array_ref[i]);
        ASSERT_EQ(const_array_ref.get(i)->value, const_array_ref[i].value);
    }
    
    // Manual cleanup
    WBE::destroy_obj_array_align<TestObject>(allocator, array_id, 4);
    ASSERT_EQ(TestObject::instance_count, 0);
}

TEST(WBERefRawTest, IndexedAccessErrorMessages) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    // Test with single object (num = 1)
    auto single_ref = WBE::new_ref<TestObject>(&allocator, 123);
    
    // Test that error messages contain the index information
    try {
        single_ref.get(5);
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& p_e) {
        std::string error_msg = p_e.what();
        ASSERT_NE(error_msg.find("5"), std::string::npos);
        ASSERT_NE(error_msg.find("index out of bounds"), std::string::npos);
    }
    
    try {
        single_ref[10];
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& p_e) {
        std::string error_msg = p_e.what();
        ASSERT_NE(error_msg.find("10"), std::string::npos);
        ASSERT_NE(error_msg.find("index out of bounds"), std::string::npos);
    }
    
    WBE::delete_ref(std::move(single_ref));
    
    // Test with array (num = 3)
    WBE::MemID array_id = create_obj_array_align<TestObject>(allocator, 3, 456);
    WBE::RefRaw<TestObject> array_ref(array_id, 3, &allocator);
    
    try {
        array_ref.get(5);
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& p_e) {
        std::string error_msg = p_e.what();
        ASSERT_NE(error_msg.find("5"), std::string::npos);
        ASSERT_NE(error_msg.find("index out of bounds"), std::string::npos);
    }
    
    try {
        array_ref[10];
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& p_e) {
        std::string error_msg = p_e.what();
        ASSERT_NE(error_msg.find("10"), std::string::npos);
        ASSERT_NE(error_msg.find("index out of bounds"), std::string::npos);
    }
    
    // Manual cleanup
    WBE::destroy_obj_array_align<TestObject>(allocator, array_id, 3);
    ASSERT_EQ(TestObject::instance_count, 0);
}

TEST(WBERefRawTest, IndexedAccessWithTemplateConversions) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    // Create array of DerivedTestObject
    WBE::MemID array_id = create_obj_array_align<DerivedTestObject>(allocator, 3, 100);
    WBE::RefRaw<DerivedTestObject> derived_ref(array_id, 3, &allocator);
    
    // Set different values for each element
    derived_ref[0].value = 10;
    derived_ref[1].value = 30;
    derived_ref[2].value = 50;
    
    // Convert to base class reference
    WBE::RefRaw<TestObject> base_ref(derived_ref);
    
    // Test indexed access through base reference
    ASSERT_EQ(base_ref[0].value, 10);
    ASSERT_EQ(base_ref[1].value, 30);
    ASSERT_EQ(base_ref[2].value, 50);
    
    // Test get() with index through base reference
    ASSERT_EQ(base_ref.get(0)->value, 10);
    ASSERT_EQ(base_ref.get(1)->value, 30);
    ASSERT_EQ(base_ref.get(2)->value, 50);
    
    // Test out of bounds on converted reference
    ASSERT_THROW(base_ref[3], std::runtime_error);
    ASSERT_THROW(base_ref.get(3), std::runtime_error);
    
    // Manual cleanup
    WBE::destroy_obj_array_align<DerivedTestObject>(allocator, array_id, 3);
    ASSERT_EQ(TestObject::instance_count, 0);
}

TEST(WBERefRawTest, IndexedAccessNumFieldConsistency) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    TestObject::instance_count = 0;
    
    // Test that indexed access respects the num field correctly
    WBE::MemID array_id = create_obj_array_align<TestObject>(allocator, 10, 777);
    
    // Create RefRaw with smaller num than actual array size
    WBE::RefRaw<TestObject> limited_ref(array_id, 5, &allocator);  // Only access first 5 elements
    
    // Should be able to access indices 0-4
    for (size_t i = 0; i < 5; ++i) {
        ASSERT_NO_THROW(limited_ref.get(i));
        ASSERT_NO_THROW(limited_ref[i]);
        ASSERT_EQ(limited_ref.get(i)->value, 777);
        ASSERT_EQ(limited_ref[i].value, 777);
    }
    
    // Should not be able to access index 5 and beyond
    ASSERT_THROW(limited_ref.get(5), std::runtime_error);
    ASSERT_THROW(limited_ref[5], std::runtime_error);
    ASSERT_THROW(limited_ref.get(9), std::runtime_error);
    ASSERT_THROW(limited_ref[9], std::runtime_error);
    
    // Test that get_num() returns the limited number
    ASSERT_EQ(limited_ref.get_num(), 5);
    
    // Manual cleanup of full array
    WBE::destroy_obj_array_align<TestObject>(allocator, array_id, 10);
    ASSERT_EQ(TestObject::instance_count, 0);
}

TEST(WBERefRawTest, ObjTypeAlias) {
    // Test that ObjType alias works correctly
    static_assert(std::is_same_v<WBE::RefRaw<TestObject>::ObjType, TestObject>);
    static_assert(std::is_same_v<WBE::RefRaw<int>::ObjType, int>);
}

TEST(WBERefRawTest, IsNullMethod_BasicBehavior) {
    WBE::MockHeapAllocatorAligned allocator(1024);
    // Default constructed should be null
    WBE::RefRaw<TestObject> default_ref;
    ASSERT_TRUE(default_ref.is_null());

    // After creating with allocator, should not be null
    auto r = WBE::new_ref<TestObject>(&allocator, 11);
    ASSERT_FALSE(r.is_null());
    ASSERT_NE(r.get(), nullptr);

    // After delete_ref, should be null again
    WBE::delete_ref(std::move(r));
    ASSERT_TRUE(r.is_null());
    ASSERT_EQ(r.get(), nullptr);

    // const version
    WBE::RefRaw<const TestObject> const_default_ref;
    ASSERT_TRUE(const_default_ref.is_null());
}

#endif
