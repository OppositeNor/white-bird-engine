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
#ifndef WBE_FILE_JOB_HH
#define WBE_FILE_JOB_HH

#include "utils/defs.hh"

namespace WhiteBirdEngine {

/**
 * @class Job
 * @brief A job.
 *
 */
template <typename ChildT>
class Job {
public:
    WBE_R6_DEFAULT_VIRTUAL(Job)

    /**
     * @brief Get this job instance with the child type.
     *
     * @return The instance with the child type.
     */
    ChildT* get() {
        return static_cast<ChildT*>(this);
    }
};

} // namespace WhiteBirdEngine

#endif
