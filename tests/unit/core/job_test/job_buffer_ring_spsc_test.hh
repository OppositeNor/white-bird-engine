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
#ifndef __WBE_JOB_BUFFER_RING_SPSC_TEST_HH__
#define __WBE_JOB_BUFFER_RING_SPSC_TEST_HH__

#include "core/job/job.hh"
#include "core/job/job_buffer_ring_spsc.hh"
#include "global/global.hh"
#include "platform/file_system/directory.hh"
#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <vector>

namespace WBE = WhiteBirdEngine;

// Mock job for testing
class MockJob : public WBE::Job<MockJob> {
public:
    MockJob(int id = 0) : job_id(id), performed(false) {}
    
    void perform() {
        performed = true;
        perform_count.fetch_add(1);
    }
    
    int job_id;
    bool performed;
    inline static std::atomic<int> perform_count;
};

class WBEJobBufferRingSPSCTest : public ::testing::Test {
protected:
    void SetUp() override {
        global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
        MockJob::perform_count.store(0);
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

using JobBufferRingSPSC = WBE::JobBufferRingSPSC<MockJob>;

TEST_F(WBEJobBufferRingSPSCTest, ConstructorValidSize) {
    // Should succeed with size >= 2
    EXPECT_NO_THROW({
        JobBufferRingSPSC buffer(get_allocator(), 2);
    });
    
    EXPECT_NO_THROW({
        JobBufferRingSPSC buffer(get_allocator(), 10);
    });
    
    EXPECT_NO_THROW({
        JobBufferRingSPSC buffer(get_allocator(), 1000);
    });
}

TEST_F(WBEJobBufferRingSPSCTest, ConstructorInvalidSize) {
    // Should throw with size < 2
    EXPECT_THROW({
        JobBufferRingSPSC buffer(get_allocator(), 0);
    }, std::runtime_error);
    
    EXPECT_THROW({
        JobBufferRingSPSC buffer(get_allocator(), 1);
    }, std::runtime_error);
}

TEST_F(WBEJobBufferRingSPSCTest, RetrieveFromEmptyBuffer) {
    JobBufferRingSPSC buffer(get_allocator(), 5);
    
    // Empty buffer should return MEM_NULL
    WBE::Ref<MockJob> job = buffer.retrieve_job();
    EXPECT_EQ(job, WBE::MEM_NULL);
}

TEST_F(WBEJobBufferRingSPSCTest, AddAndRetrieveSingleJob) {
    JobBufferRingSPSC buffer(get_allocator(), 5);
    
    auto mock_job = WBE::make_ref<MockJob>(get_allocator(), 1);
    
    // Add job
    EXPECT_NO_THROW(buffer.add_job(mock_job));
    
    // Retrieve job
    WBE::Ref<MockJob> retrieved = buffer.retrieve_job();
    EXPECT_NE(retrieved, WBE::MEM_NULL);
    
    // Should be the same job - cast back to MockJob to verify
    auto retrieved_mock = retrieved.dynamic_cast_ref<MockJob>();
    EXPECT_NE(retrieved_mock, WBE::MEM_NULL);
    EXPECT_EQ(retrieved_mock->job_id, 1);
    EXPECT_FALSE(retrieved_mock->performed);
    
    // Buffer should be empty again
    WBE::Ref<MockJob> empty = buffer.retrieve_job();
    EXPECT_EQ(empty, WBE::MEM_NULL);
}

TEST_F(WBEJobBufferRingSPSCTest, AddAndRetrieveMultipleJobs) {
    JobBufferRingSPSC buffer(get_allocator(), 5);
    
    // Add multiple jobs
    std::vector<WBE::Ref<MockJob>> jobs;
    for (int i = 0; i < 3; ++i) {
        auto job = WBE::make_ref<MockJob>(get_allocator(), i);
        jobs.push_back(job);
        buffer.add_job(job);
    }
    
    // Retrieve jobs (should be in FIFO order)
    for (int i = 0; i < 3; ++i) {
        WBE::Ref<MockJob> retrieved = buffer.retrieve_job();
        EXPECT_NE(retrieved, WBE::MEM_NULL);
        
        auto retrieved_mock = retrieved.dynamic_cast_ref<MockJob>();
        EXPECT_NE(retrieved_mock, WBE::MEM_NULL);
        EXPECT_EQ(retrieved_mock->job_id, i);
    }
    
    // Buffer should be empty
    WBE::Ref<MockJob> empty = buffer.retrieve_job();
    EXPECT_EQ(empty, WBE::MEM_NULL);
}

TEST_F(WBEJobBufferRingSPSCTest, BufferOverflow) {
    JobBufferRingSPSC buffer(get_allocator(), 3);  // Small buffer
    
    // Add jobs until buffer is full (size - 1 due to ring buffer implementation)
    auto job1 = WBE::make_ref<MockJob>(get_allocator(), 1);
    auto job2 = WBE::make_ref<MockJob>(get_allocator(), 2);
    
    EXPECT_NO_THROW(buffer.add_job(job1));
    EXPECT_NO_THROW(buffer.add_job(job2));
    
    // This should cause overflow
    auto job3 = WBE::make_ref<MockJob>(get_allocator(), 3);
    EXPECT_THROW(buffer.add_job(job3), std::runtime_error);
}

TEST_F(WBEJobBufferRingSPSCTest, RingBufferWrapAround) {
    JobBufferRingSPSC buffer(get_allocator(), 4);
    
    // Fill buffer
    auto job1 = WBE::make_ref<MockJob>(get_allocator(), 1);
    auto job2 = WBE::make_ref<MockJob>(get_allocator(), 2);
    auto job3 = WBE::make_ref<MockJob>(get_allocator(), 3);
    
    buffer.add_job(job1);
    buffer.add_job(job2);
    buffer.add_job(job3);
    
    // Retrieve one job to make space
    WBE::Ref<MockJob> retrieved = buffer.retrieve_job();
    auto retrieved_mock = retrieved.dynamic_cast_ref<MockJob>();
    EXPECT_EQ(retrieved_mock->job_id, 1);
    
    // Now we should be able to add another job (wrap around)
    auto job4 = WBE::make_ref<MockJob>(get_allocator(), 4);
    EXPECT_NO_THROW(buffer.add_job(job4));
    
    // Verify order is maintained
    retrieved = buffer.retrieve_job();
    retrieved_mock = retrieved.dynamic_cast_ref<MockJob>();
    EXPECT_EQ(retrieved_mock->job_id, 2);
    
    retrieved = buffer.retrieve_job();
    retrieved_mock = retrieved.dynamic_cast_ref<MockJob>();
    EXPECT_EQ(retrieved_mock->job_id, 3);
    
    retrieved = buffer.retrieve_job();
    retrieved_mock = retrieved.dynamic_cast_ref<MockJob>();
    EXPECT_EQ(retrieved_mock->job_id, 4);
}

TEST_F(WBEJobBufferRingSPSCTest, FIFOOrdering) {
    JobBufferRingSPSC buffer(get_allocator(), 10);
    
    // Add jobs with specific IDs
    for (int i = 100; i < 105; ++i) {
        auto job = WBE::make_ref<MockJob>(get_allocator(), i);
        buffer.add_job(job);
    }
    
    // Retrieve jobs and verify FIFO order
    for (int i = 100; i < 105; ++i) {
        WBE::Ref<MockJob> retrieved = buffer.retrieve_job();
        EXPECT_NE(retrieved, WBE::MEM_NULL);
        
        auto retrieved_mock = retrieved.dynamic_cast_ref<MockJob>();
        EXPECT_EQ(retrieved_mock->job_id, i);
    }
}

TEST_F(WBEJobBufferRingSPSCTest, MixedAddRetrieveOperations) {
    JobBufferRingSPSC buffer(get_allocator(), 5);
    
    // Add a job
    auto job1 = WBE::make_ref<MockJob>(get_allocator(), 1);
    buffer.add_job(job1);
    
    // Retrieve it
    WBE::Ref<MockJob> retrieved = buffer.retrieve_job();
    auto retrieved_mock = retrieved.dynamic_cast_ref<MockJob>();
    EXPECT_EQ(retrieved_mock->job_id, 1);
    
    // Add more jobs
    auto job2 = WBE::make_ref<MockJob>(get_allocator(), 2);
    auto job3 = WBE::make_ref<MockJob>(get_allocator(), 3);
    buffer.add_job(job2);
    buffer.add_job(job3);
    
    // Retrieve one
    retrieved = buffer.retrieve_job();
    retrieved_mock = retrieved.dynamic_cast_ref<MockJob>();
    EXPECT_EQ(retrieved_mock->job_id, 2);
    
    // Add another
    auto job4 = WBE::make_ref<MockJob>(get_allocator(), 4);
    buffer.add_job(job4);
    
    // Retrieve remaining
    retrieved = buffer.retrieve_job();
    retrieved_mock = retrieved.dynamic_cast_ref<MockJob>();
    EXPECT_EQ(retrieved_mock->job_id, 3);
    
    retrieved = buffer.retrieve_job();
    retrieved_mock = retrieved.dynamic_cast_ref<MockJob>();
    EXPECT_EQ(retrieved_mock->job_id, 4);
    
    // Should be empty
    EXPECT_EQ(buffer.retrieve_job(), WBE::MEM_NULL);
}

TEST_F(WBEJobBufferRingSPSCTest, JobExecution) {
    JobBufferRingSPSC buffer(get_allocator(), 5);
    
    auto job = WBE::make_ref<MockJob>(get_allocator(), 42);
    buffer.add_job(job);
    
    WBE::Ref<MockJob> retrieved = buffer.retrieve_job();
    EXPECT_NE(retrieved, WBE::MEM_NULL);
    
    auto mock_job = retrieved.dynamic_cast_ref<MockJob>();
    EXPECT_FALSE(mock_job->performed);
    
    // Execute the job
    retrieved->perform();
    EXPECT_TRUE(mock_job->performed);
}

// SPSC (Single Producer Single Consumer) concurrent tests
TEST_F(WBEJobBufferRingSPSCTest, ConcurrentProducerConsumer) {
    constexpr int BUFFER_SIZE = 100;
    constexpr int NUM_JOBS = 1000;
    
    JobBufferRingSPSC buffer(get_allocator(), BUFFER_SIZE);
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    std::atomic<bool> producer_done{false};
    
    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < NUM_JOBS; ++i) {
            auto job = WBE::make_ref<MockJob>(get_allocator(), i);
            
            // Keep trying until we can add the job (buffer might be full)
            while (true) {
                try {
                    buffer.add_job(job);
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
        int last_job_id = -1;
        while (!producer_done || consumed.load() < produced.load()) {
            WBE::Ref<MockJob> job = buffer.retrieve_job();
            if (job != WBE::MEM_NULL) {
                auto mock_job = job.dynamic_cast_ref<MockJob>();
                
                // Verify ordering (jobs should come in sequence)
                EXPECT_GT(mock_job->job_id, last_job_id);
                last_job_id = mock_job->job_id;
                
                job->perform();
                consumed.fetch_add(1);
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    EXPECT_EQ(produced.load(), NUM_JOBS);
    EXPECT_EQ(consumed.load(), NUM_JOBS);
    EXPECT_EQ(MockJob::perform_count.load(), NUM_JOBS);
}

TEST_F(WBEJobBufferRingSPSCTest, ProducerConsumerWithSmallBuffer) {
    constexpr int BUFFER_SIZE = 5;  // Very small buffer
    constexpr int NUM_JOBS = 100;
    
    JobBufferRingSPSC buffer(get_allocator(), BUFFER_SIZE);
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    std::atomic<bool> producer_done{false};
    
    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < NUM_JOBS; ++i) {
            auto job = WBE::make_ref<MockJob>(get_allocator(), i);
            
            while (true) {
                try {
                    buffer.add_job(job);
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
        while (!producer_done || consumed.load() < NUM_JOBS) {
            WBE::Ref<MockJob> job = buffer.retrieve_job();
            if (job != WBE::MEM_NULL) {
                job->perform();
                consumed.fetch_add(1);
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    EXPECT_EQ(produced.load(), NUM_JOBS);
    EXPECT_EQ(consumed.load(), NUM_JOBS);
}

TEST_F(WBEJobBufferRingSPSCTest, StressTestManyOperations) {
    constexpr int BUFFER_SIZE = 50;
    constexpr int NUM_JOBS = 10000;
    
    JobBufferRingSPSC buffer(get_allocator(), BUFFER_SIZE);
    std::vector<int> consumed_job_ids;
    std::atomic<bool> producer_done{false};
    
    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < NUM_JOBS; ++i) {
            auto job = WBE::make_ref<MockJob>(get_allocator(), i);
            
            while (true) {
                try {
                    buffer.add_job(job);
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
        while (!producer_done || consumed_job_ids.size() < NUM_JOBS) {
            WBE::Ref<MockJob> job = buffer.retrieve_job();
            if (job != WBE::MEM_NULL) {
                auto mock_job = job.dynamic_cast_ref<MockJob>();
                consumed_job_ids.push_back(mock_job->job_id);
                job->perform();
                
                if (consumed_job_ids.size() >= NUM_JOBS) {
                    break;
                }
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    // Verify all jobs were consumed in correct order
    EXPECT_EQ(consumed_job_ids.size(), NUM_JOBS);
    for (int i = 0; i < NUM_JOBS; ++i) {
        EXPECT_EQ(consumed_job_ids[i], i);
    }
}

#endif
