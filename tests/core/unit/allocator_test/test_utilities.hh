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
#ifndef __WBE_TEST_UTILITIES_HH__
#define __WBE_TEST_UTILITIES_HH__
#include <sstream>
#include <string>
#include <vector>

class WBEAllocPoolBehavTestClass {
public:
    WBEAllocPoolBehavTestClass(const std::string& p_pool_name, size_t p_total_size, size_t p_header_size)
        : pool_name(p_pool_name), total_size(p_total_size), header_size(p_header_size) {}

    std::string operator()(const std::vector<std::pair<bool, int64_t>>& p_chunk_status) {
        std::stringstream result;
        result << R"({"type":")" << pool_name << R"(","total_size":)" << total_size << R"(,"free_chunk_layout":[)";
        size_t tracker = 0;
        bool is_first = true;
        for (auto& status : p_chunk_status) {
            if (status.first) {
                if (status.second < 0) {
                    tracker = header_size;
                }
                else {
                    tracker += status.second + header_size;
                }
                continue;
            }
            if (!is_first) {
                result << ",";
            }
            is_first = false;
            if (status.second < 0) {
                result << R"({"begin":)" << tracker << R"(,"size":)" << total_size - tracker <<"}";
                tracker = total_size;
            }
            else {
                result << R"({"begin":)" << tracker << R"(,"size":)" << status.second <<"}";
                tracker += status.second;
            }
        }
        result << "]}";
        return result.str();
    }

    std::string pool_name;
    size_t total_size, header_size;
};

#endif

