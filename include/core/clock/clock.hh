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
#ifndef __WBE_CLOCK_HH__
#define __WBE_CLOCK_HH__

#include <chrono>
namespace WhiteBirdEngine {

/**
 * @class Clock
 * @brief The global clock.
 *
 */
class Clock {
public:
    Clock() {
        start_time = std::chrono::high_resolution_clock::now();
    }
    ~Clock() {}
    Clock(const Clock& p_other)
        : start_time(p_other.start_time) {}
    Clock(Clock&& p_other)
        : start_time(std::move(p_other.start_time)) {}
    Clock& operator=(const Clock& p_other) {
        start_time = p_other.start_time;
        return *this;
    }
    Clock& operator=(Clock&& p_other) {
        start_time = std::move(p_other.start_time);
        return *this;
    }

    /**
     * @brief Get the duration from construction.
     *
     * @return The duration in seconds.
     */
    double get_duration() const {
        std::chrono::duration<double> duration = std::chrono::high_resolution_clock::now() - start_time;
        return duration.count();
    }

private:
    std::chrono::high_resolution_clock::time_point start_time;

};

}

#endif
