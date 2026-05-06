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
#include "core/async/worker.hh"
#include "platform/file_system/path.hh"
#include "utils/utils.hh"
#include <functional>
#include <future>
#include <string>
#include <utility>
#include <vector>

namespace WhiteBirdEngine {

void Worker::add_job(std::function<void()> p_job) {
    job_buffer->add_job(std::move(p_job));
}

void Worker::run() {
    while (!should_exit) {
        auto job = job_buffer->retrieve_job(true);
        if (job != nullptr) {
            job();
        }
    }
}

void Worker::read_text_file(std::promise<std::string>& p_promise, const Path& p_path) {
    add_job([&]() {
        p_promise.set_value(load_text_file(static_cast<std::string>(p_path).c_str()));
    });
}

void Worker::read_binary_file(std::promise<std::vector<char>>& p_promise, const Path& p_path) {
    add_job([&]() {
        p_promise.set_value(load_binary_file(static_cast<std::string>(p_path).c_str()));
    });
}

} // namespace WhiteBirdEngine
