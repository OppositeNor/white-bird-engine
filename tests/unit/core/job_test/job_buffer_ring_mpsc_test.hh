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
#ifndef WBE_FILE_JOB_BUFFER_RING_MPSC_TEST_HH
#define WBE_FILE_JOB_BUFFER_RING_MPSC_TEST_HH

#include "core/job/job_buffer_ring_mpsc.hh"
#include "global/global.hh"
#include "platform/file_system/directory.hh"
#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <vector>
#include <numeric>
#include <mutex>

namespace WBE = WhiteBirdEngine;

class WBEJobBufferRingMPSCTest : public ::testing::Test {
protected:
    void SetUp() override {
        global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    }

    void TearDown() override {
        global.reset();
    }

    std::unique_ptr<WBE::Global> global;
    
    // Helper to get allocator from global
    WBE::HeapAllocatorDefault* get_allocator() {
        return global->engine_core->pool_allocator;
    }

    WBE::HeapAllocatorAtomicDefault* get_atomic_allocator() {
        return global->engine_core->atomic_pool_allocator;
    }
};

using JobBufferRingMPSC = WBE::JobBufferRingMPSC;

TEST_F(WBEJobBufferRingMPSCTest, ConstructorValidSize) {
    // Should succeed with size >= 2
    EXPECT_NO_THROW({
        JobBufferRingMPSC buffer(get_allocator(), 2);
    });
    
    EXPECT_NO_THROW({
        JobBufferRingMPSC buffer(get_allocator(), 10);
    });
    
    EXPECT_NO_THROW({
        JobBufferRingMPSC buffer(get_allocator(), 1000);
    });
}

TEST_F(WBEJobBufferRingMPSCTest, ConstructorInvalidSize) {
    // Should throw with size < 2
    EXPECT_THROW({
        JobBufferRingMPSC buffer(get_allocator(), 0);
    }, std::runtime_error);
    
    EXPECT_THROW({
        JobBufferRingMPSC buffer(get_allocator(), 1);
    }, std::runtime_error);
}

TEST_F(WBEJobBufferRingMPSCTest, RetrieveFromEmptyBuffer) {
    JobBufferRingMPSC buffer(get_allocator(), 5);
    
    // Empty buffer should return MEM_NULL
    WBE::Ref<WBE::Job> job = buffer.retrieve_job();
    EXPECT_EQ(job, WBE::MEM_NULL);
}

TEST_F(WBEJobBufferRingMPSCTest, AddAndRetrieveSingleJob) {
    JobBufferRingMPSC buffer(get_allocator(), 5);
    
    bool performed = false;
    auto job = WBE::make_ref<WBE::Job>(get_allocator(), [&performed]() { performed = true; });
    
    // Add job
    EXPECT_NO_THROW(buffer.add_job(job));
    
    // Retrieve job
    WBE::Ref<WBE::Job> retrieved = buffer.retrieve_job();
    EXPECT_NE(retrieved, WBE::MEM_NULL);
    
    // Perform the job
    EXPECT_FALSE(performed);
    retrieved->perform();
    EXPECT_TRUE(performed);
    
    // Buffer should be empty again
    WBE::Ref<WBE::Job> empty = buffer.retrieve_job();
    EXPECT_EQ(empty, WBE::MEM_NULL);
}

TEST_F(WBEJobBufferRingMPSCTest, AddAndRetrieveMultipleJobs) {
    JobBufferRingMPSC buffer(get_allocator(), 5);
    
    // Add multiple jobs
    std::vector<bool> performed(3, false);
    for (int i = 0; i < 3; ++i) {
        auto job = WBE::make_ref<WBE::Job>(get_allocator(), [&performed, i]() { performed[i] = true; });
        buffer.add_job(job);
    }
    
    // Retrieve jobs (should be in FIFO order)
    for (int i = 0; i < 3; ++i) {
        WBE::Ref<WBE::Job> retrieved = buffer.retrieve_job();
        EXPECT_NE(retrieved, WBE::MEM_NULL);
        
        EXPECT_FALSE(performed[i]);
        retrieved->perform();
        EXPECT_TRUE(performed[i]);
    }
    
    // Buffer should be empty
    WBE::Ref<WBE::Job> empty = buffer.retrieve_job();
    EXPECT_EQ(empty, WBE::MEM_NULL);
}

TEST_F(WBEJobBufferRingMPSCTest, BufferOverflow) {
    JobBufferRingMPSC buffer(get_allocator(), 3);  // Small buffer
    
    auto job1 = WBE::make_ref<WBE::Job>(get_allocator(), []() {});
    auto job2 = WBE::make_ref<WBE::Job>(get_allocator(), []() {});
    
    EXPECT_NO_THROW(buffer.add_job(job1));
    EXPECT_NO_THROW(buffer.add_job(job2));
    
    // This should cause overflow
    auto job3 = WBE::make_ref<WBE::Job>(get_allocator(), []() {});
    EXPECT_THROW(buffer.add_job(job3), std::runtime_error);
}

TEST_F(WBEJobBufferRingMPSCTest, RingBufferWrapAround) {
    JobBufferRingMPSC buffer(get_allocator(), 4);
    
    auto job1 = WBE::make_ref<WBE::Job>(get_allocator(), []() {});
    auto job2 = WBE::make_ref<WBE::Job>(get_allocator(), []() {});
    auto job3 = WBE::make_ref<WBE::Job>(get_allocator(), []() {});
    
    buffer.add_job(job1);
    buffer.add_job(job2);
    buffer.add_job(job3);
    
    WBE::Ref<WBE::Job> retrieved = buffer.retrieve_job();
    EXPECT_NE(retrieved, WBE::MEM_NULL);
    
    auto job4 = WBE::make_ref<WBE::Job>(get_allocator(), []() {});
    EXPECT_NO_THROW(buffer.add_job(job4));
    
    retrieved = buffer.retrieve_job();
    EXPECT_NE(retrieved, WBE::MEM_NULL);
    
    retrieved = buffer.retrieve_job();
    EXPECT_NE(retrieved, WBE::MEM_NULL);
    
    retrieved = buffer.retrieve_job();
    EXPECT_NE(retrieved, WBE::MEM_NULL);
}

TEST_F(WBEJobBufferRingMPSCTest, ConcurrentMultiProducerSingleConsumer) {
    constexpr int BUFFER_SIZE = 100;
    constexpr int NUM_PRODUCERS = 4;
    constexpr int JOBS_PER_PRODUCER = 250;
    constexpr int TOTAL_JOBS = NUM_PRODUCERS * JOBS_PER_PRODUCER;

    JobBufferRingMPSC buffer(get_allocator(), BUFFER_SIZE);
    std::atomic<int> produced_count{0};
    std::atomic<int> perform_count{0};
    std::vector<std::thread> producers;
    std::mutex consumed_mutex;

    producers.reserve(NUM_PRODUCERS);
    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        producers.emplace_back([&, producer_id = i]() {
            for (int j = 0; j < JOBS_PER_PRODUCER; ++j) {
                // Unique job ID for each job across all producers
                int job_id = producer_id * JOBS_PER_PRODUCER + j;
                auto job = WBE::make_ref<WBE::Job>(get_atomic_allocator(), [job_id, &consumed_job_ids, &perform_count, &consumed_mutex]() {
                    std::lock_guard<std::mutex> lock(consumed_mutex);
                    consumed_job_ids.push_back(job_id);
                    perform_count.fetch_add(1);
                });
                
                while (true) {
                    try {
                        buffer.add_job(job);
                        produced_count.fetch_add(1);
                        break;
                    } catch (const std::runtime_error&) {
                        std::this_thread::yield();
                    }
                }
            }
        });
    }

    std::vector<int> consumed_job_ids;
    consumed_job_ids.reserve(TOTAL_JOBS);
    int consumed_count = 0;

    // Consumer logic
    while (consumed_count < TOTAL_JOBS) {
        WBE::Ref<WBE::Job> job = buffer.retrieve_job();
        if (job != WBE::MEM_NULL) {
            job->perform();
            consumed_count++;
        } else {
            // If all producers are done and buffer is empty, break
            // A simple check, in a real scenario might need a more robust signal
            if (produced_count.load() == TOTAL_JOBS) {
                 // to avoid busy-waiting
                if (buffer.retrieve_job() == WBE::MEM_NULL) {
                    break;
                }
            }
            std::this_thread::yield();
        }
    }

    for (auto& p : producers) {
        p.join();
    }

    EXPECT_EQ(produced_count.load(), TOTAL_JOBS);
    EXPECT_EQ(consumed_count, TOTAL_JOBS);
    EXPECT_EQ(perform_count.load(), TOTAL_JOBS);

    // Verify that all jobs were consumed, regardless of order
    std::sort(consumed_job_ids.begin(), consumed_job_ids.end());
    std::vector<int> expected_ids(TOTAL_JOBS);
    std::iota(expected_ids.begin(), expected_ids.end(), 0);

    EXPECT_EQ(consumed_job_ids, expected_ids);
}

TEST_F(WBEJobBufferRingMPSCTest, StressTestManyProducers) {
    constexpr int BUFFER_SIZE = 200;
    constexpr int NUM_PRODUCERS = 10;
    constexpr int JOBS_PER_PRODUCER = 2000;
    constexpr int TOTAL_JOBS = NUM_PRODUCERS * JOBS_PER_PRODUCER;

    JobBufferRingMPSC buffer(get_allocator(), BUFFER_SIZE);
    std::atomic<int> produced_count{0};
    std::atomic<int> perform_count{0};
    std::vector<std::thread> producers;
    std::mutex consumed_mutex;

    producers.reserve(NUM_PRODUCERS);
    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        producers.emplace_back([&, producer_id = i]() {
            for (int j = 0; j < JOBS_PER_PRODUCER; ++j) {
                int job_id = producer_id * JOBS_PER_PRODUCER + j;
                auto job = WBE::make_ref<WBE::Job>(get_atomic_allocator(), [job_id, &consumed_job_ids, &perform_count, &consumed_mutex]() {
                    std::lock_guard<std::mutex> lock(consumed_mutex);
                    consumed_job_ids.push_back(job_id);
                    perform_count.fetch_add(1);
                });
                
                while (true) {
                    try {
                        buffer.add_job(job);
                        produced_count.fetch_add(1);
                        break;
                    } catch (const std::runtime_error&) {
                        std::this_thread::yield();
                    }
                }
            }
        });
    }

    std::vector<int> consumed_job_ids;
    consumed_job_ids.reserve(TOTAL_JOBS);
    int consumed_count = 0;

    while (consumed_count < TOTAL_JOBS) {
        WBE::Ref<WBE::Job> job = buffer.retrieve_job();
        if (job != WBE::MEM_NULL) {
            job->perform();
            consumed_count++;
        } else if (produced_count.load() == TOTAL_JOBS) {
            // All jobs produced, but buffer might be temporarily empty.
            // One final check to ensure we don't exit prematurely.
            job = buffer.retrieve_job();
            if (job == WBE::MEM_NULL) {
                break;
            }
            job->perform();
            consumed_count++;
        } else {
            std::this_thread::yield();
        }
    }

    for (auto& p : producers) {
        p.join();
    }

    EXPECT_EQ(produced_count.load(), TOTAL_JOBS);
    EXPECT_EQ(consumed_count, TOTAL_JOBS);
    EXPECT_EQ(perform_count.load(), TOTAL_JOBS);

    // Verify that all jobs were consumed
    std::sort(consumed_job_ids.begin(), consumed_job_ids.end());
    std::vector<int> expected_ids(TOTAL_JOBS);
    std::iota(expected_ids.begin(), expected_ids.end(), 0);

    EXPECT_EQ(consumed_job_ids, expected_ids);
}


#endif
