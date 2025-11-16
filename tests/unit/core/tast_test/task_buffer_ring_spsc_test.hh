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
#ifndef __WBE_TASK_BUFFER_RING_SPSC_TEST_HH__
#define __WBE_TASK_BUFFER_RING_SPSC_TEST_HH__

#include "core/task/task_buffer_ring_spsc.hh"
#include "global/global.hh"
#include "platform/file_system/directory.hh"
#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>

namespace WBE = WhiteBirdEngine;

// Mock task for testing
class MockTask : public WBE::Task {
public:
    MockTask(int id = 0) : task_id(id), performed(false) {}
    
    virtual void perform() override {
        performed = true;
        perform_count.fetch_add(1);
    }
    
    int task_id;
    bool performed;
    inline static std::atomic<int> perform_count;
};

class WBETaskBufferRingSPSCTest : public ::testing::Test {
protected:
    void SetUp() override {
        global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
        MockTask::perform_count.store(0);
    }

    void TearDown() override {
        global.reset();
    }

    std::unique_ptr<WBE::Global> global;
    
    // Helper to get allocator from global
    WBE::HeapAllocatorDefault* get_allocator() {
        return global->engine_core->pool_allocator;
    }
};

TEST_F(WBETaskBufferRingSPSCTest, ConstructorValidSize) {
    // Should succeed with size >= 2
    EXPECT_NO_THROW({
        WBE::TaskBufferRingSPSC buffer(get_allocator(), 2);
    });
    
    EXPECT_NO_THROW({
        WBE::TaskBufferRingSPSC buffer(get_allocator(), 10);
    });
    
    EXPECT_NO_THROW({
        WBE::TaskBufferRingSPSC buffer(get_allocator(), 1000);
    });
}

TEST_F(WBETaskBufferRingSPSCTest, ConstructorInvalidSize) {
    // Should throw with size < 2
    EXPECT_THROW({
        WBE::TaskBufferRingSPSC buffer(get_allocator(), 0);
    }, std::runtime_error);
    
    EXPECT_THROW({
        WBE::TaskBufferRingSPSC buffer(get_allocator(), 1);
    }, std::runtime_error);
}

TEST_F(WBETaskBufferRingSPSCTest, RetrieveFromEmptyBuffer) {
    WBE::TaskBufferRingSPSC buffer(get_allocator(), 5);
    
    // Empty buffer should return MEM_NULL
    WBE::Ref<WBE::Task> task = buffer.retrieve_task();
    EXPECT_EQ(task, WBE::MEM_NULL);
}

TEST_F(WBETaskBufferRingSPSCTest, AddAndRetrieveSingleTask) {
    WBE::TaskBufferRingSPSC buffer(get_allocator(), 5);
    
    auto mock_task = WBE::make_ref<MockTask>(get_allocator(), 1);
    
    // Add task
    EXPECT_NO_THROW(buffer.add_task(mock_task));
    
    // Retrieve task
    WBE::Ref<WBE::Task> retrieved = buffer.retrieve_task();
    EXPECT_NE(retrieved, WBE::MEM_NULL);
    
    // Should be the same task - cast back to MockTask to verify
    auto retrieved_mock = retrieved.dynamic_cast_ref<MockTask>();
    EXPECT_NE(retrieved_mock, WBE::MEM_NULL);
    EXPECT_EQ(retrieved_mock->task_id, 1);
    EXPECT_FALSE(retrieved_mock->performed);
    
    // Buffer should be empty again
    WBE::Ref<WBE::Task> empty = buffer.retrieve_task();
    EXPECT_EQ(empty, WBE::MEM_NULL);
}

TEST_F(WBETaskBufferRingSPSCTest, AddAndRetrieveMultipleTasks) {
    WBE::TaskBufferRingSPSC buffer(get_allocator(), 5);
    
    // Add multiple tasks
    std::vector<WBE::Ref<MockTask>> tasks;
    for (int i = 0; i < 3; ++i) {
        auto task = WBE::make_ref<MockTask>(get_allocator(), i);
        tasks.push_back(task);
        buffer.add_task(task);
    }
    
    // Retrieve tasks (should be in FIFO order)
    for (int i = 0; i < 3; ++i) {
        WBE::Ref<WBE::Task> retrieved = buffer.retrieve_task();
        EXPECT_NE(retrieved, WBE::MEM_NULL);
        
        auto retrieved_mock = retrieved.dynamic_cast_ref<MockTask>();
        EXPECT_NE(retrieved_mock, WBE::MEM_NULL);
        EXPECT_EQ(retrieved_mock->task_id, i);
    }
    
    // Buffer should be empty
    WBE::Ref<WBE::Task> empty = buffer.retrieve_task();
    EXPECT_EQ(empty, WBE::MEM_NULL);
}

TEST_F(WBETaskBufferRingSPSCTest, BufferOverflow) {
    WBE::TaskBufferRingSPSC buffer(get_allocator(), 3);  // Small buffer
    
    // Add tasks until buffer is full (size - 1 due to ring buffer implementation)
    auto task1 = WBE::make_ref<MockTask>(get_allocator(), 1);
    auto task2 = WBE::make_ref<MockTask>(get_allocator(), 2);
    
    EXPECT_NO_THROW(buffer.add_task(task1));
    EXPECT_NO_THROW(buffer.add_task(task2));
    
    // This should cause overflow
    auto task3 = WBE::make_ref<MockTask>(get_allocator(), 3);
    EXPECT_THROW(buffer.add_task(task3), std::runtime_error);
}

TEST_F(WBETaskBufferRingSPSCTest, RingBufferWrapAround) {
    WBE::TaskBufferRingSPSC buffer(get_allocator(), 4);
    
    // Fill buffer
    auto task1 = WBE::make_ref<MockTask>(get_allocator(), 1);
    auto task2 = WBE::make_ref<MockTask>(get_allocator(), 2);
    auto task3 = WBE::make_ref<MockTask>(get_allocator(), 3);
    
    buffer.add_task(task1);
    buffer.add_task(task2);
    buffer.add_task(task3);
    
    // Retrieve one task to make space
    WBE::Ref<WBE::Task> retrieved = buffer.retrieve_task();
    auto retrieved_mock = retrieved.dynamic_cast_ref<MockTask>();
    EXPECT_EQ(retrieved_mock->task_id, 1);
    
    // Now we should be able to add another task (wrap around)
    auto task4 = WBE::make_ref<MockTask>(get_allocator(), 4);
    EXPECT_NO_THROW(buffer.add_task(task4));
    
    // Verify order is maintained
    retrieved = buffer.retrieve_task();
    retrieved_mock = retrieved.dynamic_cast_ref<MockTask>();
    EXPECT_EQ(retrieved_mock->task_id, 2);
    
    retrieved = buffer.retrieve_task();
    retrieved_mock = retrieved.dynamic_cast_ref<MockTask>();
    EXPECT_EQ(retrieved_mock->task_id, 3);
    
    retrieved = buffer.retrieve_task();
    retrieved_mock = retrieved.dynamic_cast_ref<MockTask>();
    EXPECT_EQ(retrieved_mock->task_id, 4);
}

TEST_F(WBETaskBufferRingSPSCTest, FIFOOrdering) {
    WBE::TaskBufferRingSPSC buffer(get_allocator(), 10);
    
    // Add tasks with specific IDs
    for (int i = 100; i < 105; ++i) {
        auto task = WBE::make_ref<MockTask>(get_allocator(), i);
        buffer.add_task(task);
    }
    
    // Retrieve tasks and verify FIFO order
    for (int i = 100; i < 105; ++i) {
        WBE::Ref<WBE::Task> retrieved = buffer.retrieve_task();
        EXPECT_NE(retrieved, WBE::MEM_NULL);
        
        auto retrieved_mock = retrieved.dynamic_cast_ref<MockTask>();
        EXPECT_EQ(retrieved_mock->task_id, i);
    }
}

TEST_F(WBETaskBufferRingSPSCTest, MixedAddRetrieveOperations) {
    WBE::TaskBufferRingSPSC buffer(get_allocator(), 5);
    
    // Add a task
    auto task1 = WBE::make_ref<MockTask>(get_allocator(), 1);
    buffer.add_task(task1);
    
    // Retrieve it
    WBE::Ref<WBE::Task> retrieved = buffer.retrieve_task();
    auto retrieved_mock = retrieved.dynamic_cast_ref<MockTask>();
    EXPECT_EQ(retrieved_mock->task_id, 1);
    
    // Add more tasks
    auto task2 = WBE::make_ref<MockTask>(get_allocator(), 2);
    auto task3 = WBE::make_ref<MockTask>(get_allocator(), 3);
    buffer.add_task(task2);
    buffer.add_task(task3);
    
    // Retrieve one
    retrieved = buffer.retrieve_task();
    retrieved_mock = retrieved.dynamic_cast_ref<MockTask>();
    EXPECT_EQ(retrieved_mock->task_id, 2);
    
    // Add another
    auto task4 = WBE::make_ref<MockTask>(get_allocator(), 4);
    buffer.add_task(task4);
    
    // Retrieve remaining
    retrieved = buffer.retrieve_task();
    retrieved_mock = retrieved.dynamic_cast_ref<MockTask>();
    EXPECT_EQ(retrieved_mock->task_id, 3);
    
    retrieved = buffer.retrieve_task();
    retrieved_mock = retrieved.dynamic_cast_ref<MockTask>();
    EXPECT_EQ(retrieved_mock->task_id, 4);
    
    // Should be empty
    EXPECT_EQ(buffer.retrieve_task(), WBE::MEM_NULL);
}

TEST_F(WBETaskBufferRingSPSCTest, TaskExecution) {
    WBE::TaskBufferRingSPSC buffer(get_allocator(), 5);
    
    auto task = WBE::make_ref<MockTask>(get_allocator(), 42);
    buffer.add_task(task);
    
    WBE::Ref<WBE::Task> retrieved = buffer.retrieve_task();
    EXPECT_NE(retrieved, WBE::MEM_NULL);
    
    auto mock_task = retrieved.dynamic_cast_ref<MockTask>();
    EXPECT_FALSE(mock_task->performed);
    
    // Execute the task
    retrieved->perform();
    EXPECT_TRUE(mock_task->performed);
}

// SPSC (Single Producer Single Consumer) concurrent tests
TEST_F(WBETaskBufferRingSPSCTest, ConcurrentProducerConsumer) {
    constexpr int BUFFER_SIZE = 100;
    constexpr int NUM_TASKS = 1000;
    
    WBE::TaskBufferRingSPSC buffer(get_allocator(), BUFFER_SIZE);
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    std::atomic<bool> producer_done{false};
    
    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < NUM_TASKS; ++i) {
            auto task = WBE::make_ref<MockTask>(get_allocator(), i);
            
            // Keep trying until we can add the task (buffer might be full)
            while (true) {
                try {
                    buffer.add_task(task);
                    produced.fetch_add(1);
                    break;
                } catch (const std::runtime_error&) {
                    // Buffer full, yield and try again
                    std::this_thread::yield();
                }
            }
        }
        producer_done = true;
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        int last_task_id = -1;
        while (!producer_done || consumed.load() < produced.load()) {
            WBE::Ref<WBE::Task> task = buffer.retrieve_task();
            if (task != WBE::MEM_NULL) {
                auto mock_task = task.dynamic_cast_ref<MockTask>();
                
                // Verify ordering (tasks should come in sequence)
                EXPECT_GT(mock_task->task_id, last_task_id);
                last_task_id = mock_task->task_id;
                
                task->perform();
                consumed.fetch_add(1);
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    EXPECT_EQ(produced.load(), NUM_TASKS);
    EXPECT_EQ(consumed.load(), NUM_TASKS);
    EXPECT_EQ(MockTask::perform_count.load(), NUM_TASKS);
}

TEST_F(WBETaskBufferRingSPSCTest, ProducerConsumerWithSmallBuffer) {
    constexpr int BUFFER_SIZE = 5;  // Very small buffer
    constexpr int NUM_TASKS = 100;
    
    WBE::TaskBufferRingSPSC buffer(get_allocator(), BUFFER_SIZE);
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    std::atomic<bool> producer_done{false};
    
    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < NUM_TASKS; ++i) {
            auto task = WBE::make_ref<MockTask>(get_allocator(), i);
            
            while (true) {
                try {
                    buffer.add_task(task);
                    produced.fetch_add(1);
                    break;
                } catch (const std::runtime_error&) {
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
            }
        }
        producer_done = true;
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        while (!producer_done || consumed.load() < NUM_TASKS) {
            WBE::Ref<WBE::Task> task = buffer.retrieve_task();
            if (task != WBE::MEM_NULL) {
                task->perform();
                consumed.fetch_add(1);
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    EXPECT_EQ(produced.load(), NUM_TASKS);
    EXPECT_EQ(consumed.load(), NUM_TASKS);
}

TEST_F(WBETaskBufferRingSPSCTest, StressTestManyOperations) {
    constexpr int BUFFER_SIZE = 50;
    constexpr int NUM_TASKS = 10000;
    
    WBE::TaskBufferRingSPSC buffer(get_allocator(), BUFFER_SIZE);
    std::vector<int> consumed_task_ids;
    std::atomic<bool> producer_done{false};
    
    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < NUM_TASKS; ++i) {
            auto task = WBE::make_ref<MockTask>(get_allocator(), i);
            
            while (true) {
                try {
                    buffer.add_task(task);
                    break;
                } catch (const std::runtime_error&) {
                    std::this_thread::yield();
                }
            }
        }
        producer_done = true;
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        while (!producer_done || consumed_task_ids.size() < NUM_TASKS) {
            WBE::Ref<WBE::Task> task = buffer.retrieve_task();
            if (task != WBE::MEM_NULL) {
                auto mock_task = task.dynamic_cast_ref<MockTask>();
                consumed_task_ids.push_back(mock_task->task_id);
                task->perform();
                
                if (consumed_task_ids.size() >= NUM_TASKS) {
                    break;
                }
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    // Verify all tasks were consumed in correct order
    EXPECT_EQ(consumed_task_ids.size(), NUM_TASKS);
    for (int i = 0; i < NUM_TASKS; ++i) {
        EXPECT_EQ(consumed_task_ids[i], i);
    }
}

#endif
