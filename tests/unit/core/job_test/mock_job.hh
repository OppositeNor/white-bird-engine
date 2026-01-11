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
#ifndef WBE_FILE_MOCK_JOB_HH
#define WBE_FILE_MOCK_JOB_HH

#include "core/job/job.hh"
#include <atomic>

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

#endif // WBE_FILE_MOCK_JOB_HH
