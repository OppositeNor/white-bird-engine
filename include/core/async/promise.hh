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
#ifndef WBE_FILE_PROMISE_HH
#define WBE_FILE_PROMISE_HH

#include <optional>
#include <semaphore>

namespace WhiteBirdEngine {

/**
 * @class Promise
 * @brief A class for holding unresolved values that are handeled asynchronously.
 *
 * @tparam T The type of the value to be resolved.
 */
template <typename T>
class Promise final {
public:
    Promise() = default;
    ~Promise() = default;
    Promise(const Promise&) = delete;
    Promise(Promise&& p_other) noexcept
        : data(std::move(p_other.data)) {}
    Promise& operator=(const Promise&) = delete;
    Promise& operator=(Promise&& p_other) noexcept {
        data = std::move(p_other.data);
    }

    /**
     * @brief Wait till the value is resolved, and get the value.
     *
     * @return The reference to the result value.
     */
    T await() {
        data_ready_semaphore.release();
        return *data;
    }

private:
    std::optional<T> data;
    std::binary_semaphore data_ready_semaphore{0};

    /**
     * @brief Called when the value has completed resolving.
     */
    void set_data(const T& p_data) {
        data = p_data;
    }

    /**
     * @brief Called when the value has completed resolving.
     */
    void set_data(T&& p_data) {
        data = std::move(p_data);
    }
};

template <typename ...Args>
class Promises final {
public:
    // TODO
private:
    // TODO
};


} // namespace WhiteBirdEngine

#endif
