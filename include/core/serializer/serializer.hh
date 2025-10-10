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
#ifndef __WBE_SERIALIZER_HH__
#define __WBE_SERIALIZER_HH__

#include "platform/file_system/path.hh"
#include "utils/defs.hh"
#include <cstdint>
#include <string>
#include <utility>
namespace WhiteBirdEngine {

template <typename ChildT>
class Serializer {
public:
    Serializer() = default;
    ~Serializer() {}


    /**
     * @brief Push the content of a serializer into a list.
     *
     * @tparam SerializerType The type of the serializer.
     * @param p_key The key of the list.
     * @param p_serializer The serializer to get data to push.
     */
    template <typename SerializerType>
    void push_list(const std::string& p_key, const SerializerType& p_serializer) {
        static_cast<ChildT*>(this)->push_context(p_serializer);
    }

    /**
     * @brief Push a new context for the serializer context stack.
     *
     * @param p_context_name The name of the context to be pushed.
     */
    void push_context(const std::string& p_context_name) {
        static_cast<ChildT*>(this)->push_context(p_context_name);
    }

    /**
     * @brief Pop to the previous context in the context stack.
     */
    void pop_context() {
        static_cast<ChildT*>(this)->pop_context();
    }

    /**
     * @brief Register a key-value pair to the serializer.
     *
     * @tparam T The type of the value.
     * @param p_key The key to be registered.
     * @param p_value The value to be registered.
     */
    template <typename T>
    void register_serialize(const std::string& p_key, T&& p_value) {
        static_cast<ChildT*>(this)->register_serialize(p_key, std::forward<T>(p_value));
    }

    /**
     * @brief Dump the content of the serializer to a string.
     *
     * @return The string to be dumped.
     */
    std::string dump() const {
        return static_cast<const ChildT*>(this)->dump();
    }

    /**
     * @brief Dump the content of the serializer to a file.
     *
     * @param p_path The path of the file to be dumped.
     */
    void dump(const Path& p_path) const {
        static_cast<const ChildT*>(this)->dump(p_path);
    }

    /**
     * @brief Get the current context.
     *
     * @return The name of the context.
     */
    std::string get_current_context() const {
        return static_cast<const ChildT*>(this)->get_current_context();
    }

    /**
     * @brief Get the depth of the current context.
     *
     * @return The depth of the current context.
     */
    uint32_t get_context_depth() const {
        return static_cast<const ChildT*>(this)->get_context_depth();
    }
};

WBE_DECL_CRTP_CONCEPT(Serializer);

}

#endif
