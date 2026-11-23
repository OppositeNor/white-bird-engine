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
#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/gpu_vector.hh"
#include "function/render/rend_defs.hh"
#include "function/render/ro_buffer.hh"
#include "utils/utils.hh"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <stdexcept>
#include <utility>
#include <vector>

namespace WhiteBirdEngine {

namespace {

struct TestElement {
    uint32_t a;
    float b;

    bool operator==(const TestElement&) const = default;
};

class ROBufferHostTest final : public ROBuffer {
public:
    ROBufferHostTest(size_t p_size, bool p_host_visible) : memory(p_size, std::byte{0xCD}), host_visible(p_host_visible) {}
    virtual ~ROBufferHostTest() override = default;
    ROBufferHostTest(const ROBufferHostTest&) = delete;
    ROBufferHostTest(ROBufferHostTest&&) noexcept = delete;
    ROBufferHostTest& operator=(const ROBufferHostTest&) = delete;
    ROBufferHostTest& operator=(ROBufferHostTest&&) noexcept = delete;

    virtual TypeID get_type_uuid() const override {
        return static_hash("GPUVectorTestROBufferHost");
    }

    virtual size_t get_size() const override {
        return memory.size();
    }

    virtual void* get_mapped() const override {
        return host_visible ? const_cast<std::byte*>(memory.data()) : nullptr;
    }

    virtual void update_buffer(const void* p_data, size_t p_dst_offset, size_t p_size) override {
        std::memcpy(memory.data() + p_dst_offset, p_data, p_size);
    }

    uint32_t stored_byte_size() const {
        uint32_t result = 0;
        std::memcpy(&result, memory.data(), sizeof(result));
        return result;
    }

    template <typename T>
    const T* elements() const {
        return reinterpret_cast<const T*>(memory.data() + GPUVector<T>::HEADER_SIZE);
    }

private:
    std::vector<std::byte> memory;
    bool host_visible;
};

} // namespace

class WBEGPUVectorTest : public ::testing::Test {
protected:
    template <typename T>
    static size_t bytes_for(size_t p_count) {
        return GPUVector<T>::HEADER_SIZE + sizeof(T) * p_count;
    }

    Ref<ROBufferHostTest> make_buffer(size_t p_size, bool p_host_visible = true) {
        return make_ref<ROBufferHostTest>(&alloc, p_size, p_host_visible);
    }

    RendAlloc alloc{WBE_KI_B(64)};
};

TEST_F(WBEGPUVectorTest, CapacityCountsWholeElementsAfterTheHeader) {
    GPUVector<TestElement> vector(make_buffer(bytes_for<TestElement>(4) + 3));

    EXPECT_EQ(vector.capacity(), 4U);
    EXPECT_EQ(vector.size(), 0U);
}

TEST_F(WBEGPUVectorTest, ConstructorWritesTheInitialSizeIntoTheHeader) {
    Ref<ROBufferHostTest> buffer = make_buffer(bytes_for<uint32_t>(4));
    GPUVector<uint32_t> vector(buffer, 2);

    EXPECT_EQ(vector.size(), 2U);
    EXPECT_EQ(buffer->stored_byte_size(), 2 * sizeof(uint32_t));
    EXPECT_EQ(vector.get(1), 0xCDCDCDCDU);
}

TEST_F(WBEGPUVectorTest, ConstructorRejectsNullBuffer) {
    EXPECT_THROW(GPUVector<uint32_t>(Ref<ROBuffer>(MEM_NULL)), std::runtime_error);
}

TEST_F(WBEGPUVectorTest, ConstructorRejectsBufferSmallerThanTheHeader) {
    EXPECT_THROW(GPUVector<uint32_t>(make_buffer(GPUVector<uint32_t>::HEADER_SIZE - 1)), std::runtime_error);
    EXPECT_NO_THROW(GPUVector<uint32_t>(make_buffer(GPUVector<uint32_t>::HEADER_SIZE)));
}

TEST_F(WBEGPUVectorTest, ConstructorRejectsInitialSizeOverCapacity) {
    EXPECT_NO_THROW(GPUVector<uint32_t>(make_buffer(bytes_for<uint32_t>(4)), 4));
    EXPECT_THROW(GPUVector<uint32_t>(make_buffer(bytes_for<uint32_t>(4)), 5), std::runtime_error);
}

TEST_F(WBEGPUVectorTest, PushBackWritesElementsAndSizeIntoTheBuffer) {
    Ref<ROBufferHostTest> buffer = make_buffer(bytes_for<TestElement>(3));
    GPUVector<TestElement> vector(buffer);

    vector.push_back(TestElement{.a = 1, .b = 1.5F});
    const TestElement second{.a = 2, .b = 2.5F};
    vector.push_back(second);

    ASSERT_EQ(vector.size(), 2U);
    EXPECT_EQ(buffer->stored_byte_size(), 2 * sizeof(TestElement));
    EXPECT_EQ(buffer->elements<TestElement>()[0], (TestElement{.a = 1, .b = 1.5F}));
    EXPECT_EQ(buffer->elements<TestElement>()[1], second);
    EXPECT_EQ(vector.get(0), (TestElement{.a = 1, .b = 1.5F}));
    EXPECT_EQ(vector.get(1), second);
}

TEST_F(WBEGPUVectorTest, SizeFollowsTheHeaderWrittenByOthers) {
    Ref<ROBufferHostTest> buffer = make_buffer(bytes_for<uint32_t>(4));
    GPUVector<uint32_t> vector(buffer);

    const uint32_t device_written_byte_size = 3 * sizeof(uint32_t);
    buffer->update_buffer(&device_written_byte_size, 0, sizeof(device_written_byte_size));

    EXPECT_EQ(vector.size(), 3U);
}

TEST_F(WBEGPUVectorTest, SizeRoundsAPartialElementDown) {
    Ref<ROBufferHostTest> buffer = make_buffer(bytes_for<TestElement>(4));
    GPUVector<TestElement> vector(buffer);

    const uint32_t device_written_byte_size = 2 * sizeof(TestElement) + 3;
    buffer->update_buffer(&device_written_byte_size, 0, sizeof(device_written_byte_size));

    EXPECT_EQ(vector.size(), 2U);
    EXPECT_NO_THROW(vector.get(1));
    EXPECT_THROW(vector.get(2), std::runtime_error);
    vector.push_back(TestElement{.a = 3, .b = 3.5F});
    EXPECT_EQ(buffer->stored_byte_size(), 3 * sizeof(TestElement));
}

TEST_F(WBEGPUVectorTest, PushBackThrowsWhenFull) {
    GPUVector<uint32_t> vector(make_buffer(bytes_for<uint32_t>(2)));
    vector.push_back(1U);
    vector.push_back(2U);

    EXPECT_THROW(vector.push_back(3U), std::runtime_error);
    EXPECT_EQ(vector.size(), 2U);
}

TEST_F(WBEGPUVectorTest, TryPushBackReportsBufferOverflow) {
    GPUVector<uint32_t> vector(make_buffer(bytes_for<uint32_t>(1)));

    EXPECT_EQ(vector.try_push_back(7U), GPUVector<uint32_t>::Failure::OK);
    EXPECT_EQ(vector.try_push_back(8U), GPUVector<uint32_t>::Failure::BUFFER_OVERFLOW);
    EXPECT_EQ(vector.size(), 1U);
    EXPECT_EQ(vector.get(0), 7U);
}

TEST_F(WBEGPUVectorTest, ResizeGrowsAndShrinks) {
    Ref<ROBufferHostTest> buffer = make_buffer(bytes_for<uint32_t>(4));
    GPUVector<uint32_t> vector(buffer);

    vector.resize(3);
    EXPECT_EQ(vector.size(), 3U);
    vector.resize(1);
    EXPECT_EQ(vector.size(), 1U);
    vector.resize(4);
    EXPECT_EQ(vector.size(), 4U);
    EXPECT_EQ(buffer->stored_byte_size(), 4 * sizeof(uint32_t));
    vector.resize(0);
    EXPECT_EQ(vector.size(), 0U);
}

TEST_F(WBEGPUVectorTest, ResizeThrowsOverCapacity) {
    GPUVector<uint32_t> vector(make_buffer(bytes_for<uint32_t>(4)), 2);

    EXPECT_THROW(vector.resize(5), std::runtime_error);
    EXPECT_EQ(vector.size(), 2U);
}

TEST_F(WBEGPUVectorTest, TryResizeReportsBufferOverflow) {
    GPUVector<uint32_t> vector(make_buffer(bytes_for<uint32_t>(4)), 2);

    EXPECT_EQ(vector.try_resize(4), GPUVector<uint32_t>::Failure::OK);
    EXPECT_EQ(vector.try_resize(5), GPUVector<uint32_t>::Failure::BUFFER_OVERFLOW);
    EXPECT_EQ(vector.size(), 4U);
}

TEST_F(WBEGPUVectorTest, PushBackAppendsAfterShrink) {
    Ref<ROBufferHostTest> buffer = make_buffer(bytes_for<uint32_t>(4));
    GPUVector<uint32_t> vector(buffer);
    vector.push_back(1U);
    vector.push_back(2U);
    vector.push_back(3U);

    vector.resize(1);
    vector.push_back(9U);

    ASSERT_EQ(vector.size(), 2U);
    EXPECT_EQ(buffer->elements<uint32_t>()[1], 9U);
    EXPECT_EQ(buffer->elements<uint32_t>()[2], 3U);
}

TEST_F(WBEGPUVectorTest, GetReturnsReferenceIntoMappedMemory) {
    Ref<ROBufferHostTest> buffer = make_buffer(bytes_for<uint32_t>(2));
    GPUVector<uint32_t> vector(buffer);
    vector.push_back(1U);

    vector.get(0) = 42U;

    EXPECT_EQ(buffer->elements<uint32_t>()[0], 42U);
}

TEST_F(WBEGPUVectorTest, GetThrowsOutOfRange) {
    GPUVector<uint32_t> vector(make_buffer(bytes_for<uint32_t>(4)), 2);

    EXPECT_NO_THROW(vector.get(1));
    EXPECT_THROW(vector.get(2), std::runtime_error);
}

TEST_F(WBEGPUVectorTest, TryGetReportsIndexOverflow) {
    Ref<ROBufferHostTest> buffer = make_buffer(bytes_for<uint32_t>(4));
    GPUVector<uint32_t> vector(buffer, 2);

    auto in_range = vector.try_get(1);
    ASSERT_TRUE(in_range.has_value());
    EXPECT_EQ(static_cast<const void*>(in_range.value()), static_cast<const void*>(buffer->elements<uint32_t>() + 1));

    auto out_of_range = vector.try_get(2);
    ASSERT_FALSE(out_of_range.has_value());
    EXPECT_EQ(out_of_range.error(), GPUVector<uint32_t>::Failure::INDEX_OVERFLOW);
}

TEST_F(WBEGPUVectorTest, BufferWithoutHostMappingReportsNotVisible) {
    GPUVector<uint32_t> vector(make_buffer(bytes_for<uint32_t>(2), false));

    EXPECT_EQ(vector.capacity(), 2U);
    ASSERT_FALSE(vector.try_size().has_value());
    EXPECT_EQ(vector.try_size().error(), GPUVector<uint32_t>::Failure::NOT_VISIBLE);
    EXPECT_THROW(static_cast<void>(vector.size()), std::runtime_error);
    EXPECT_EQ(vector.try_push_back(1U), GPUVector<uint32_t>::Failure::NOT_VISIBLE);
    EXPECT_THROW(vector.push_back(1U), std::runtime_error);
    EXPECT_EQ(vector.try_resize(1), GPUVector<uint32_t>::Failure::NOT_VISIBLE);
    EXPECT_THROW(vector.resize(1), std::runtime_error);
    ASSERT_FALSE(vector.try_get(0).has_value());
    EXPECT_EQ(vector.try_get(0).error(), GPUVector<uint32_t>::Failure::NOT_VISIBLE);
    EXPECT_THROW(vector.get(0), std::runtime_error);
}

TEST_F(WBEGPUVectorTest, BufferWithoutHostMappingRejectsInitialSize) {
    EXPECT_NO_THROW(GPUVector<uint32_t>(make_buffer(bytes_for<uint32_t>(2), false), 0));
    EXPECT_THROW(GPUVector<uint32_t>(make_buffer(bytes_for<uint32_t>(2), false), 1), std::runtime_error);
}

TEST_F(WBEGPUVectorTest, GetBufferReturnsTheStorageBuffer) {
    Ref<ROBufferHostTest> buffer = make_buffer(bytes_for<uint32_t>(1));
    GPUVector<uint32_t> vector(buffer);

    EXPECT_EQ(vector.get_buffer().get(), static_cast<ROBuffer*>(buffer.get()));
    const GPUVector<uint32_t>& const_vector = vector;
    EXPECT_EQ(const_vector.get_buffer().get(), static_cast<const ROBuffer*>(buffer.get()));
}

TEST_F(WBEGPUVectorTest, MoveTransfersBufferAndElements) {
    Ref<ROBufferHostTest> buffer = make_buffer(bytes_for<uint32_t>(2));
    GPUVector<uint32_t> source(buffer);
    source.push_back(5U);

    GPUVector<uint32_t> destination(std::move(source));

    EXPECT_EQ(destination.size(), 1U);
    EXPECT_EQ(destination.get(0), 5U);
    EXPECT_EQ(destination.get_buffer().get(), static_cast<ROBuffer*>(buffer.get()));
    EXPECT_EQ(buffer->stored_byte_size(), sizeof(uint32_t));
}

} // namespace WhiteBirdEngine
