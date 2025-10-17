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
#ifndef __WBE_UTILS_HH__
#define __WBE_UTILS_HH__

#include "utils/defs.hh"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace WhiteBirdEngine {

// Types of encoding
enum class EncodeType {
    // Unknown encode type.
    UNKNOWN,
    // UTF8 (most recommended)
    UTF8,
    // UTF16 little endian
    UTF16LE,
    // UTF16 big endian
    UTF16BE,
    // UTF32 little endian
    UTF32LE,
    // UTF32 big endian
    UTF32BE
};

/**
 * @brief Get the size after alignment.
 *
 * @param p_size Size.
 * @param p_alignment Alignment. If alignment is 0, no alignment will be made.
 * @return The size after alignment.
 */
constexpr size_t get_align_size(size_t p_size, size_t p_alignment) {
    return p_alignment == 0 ? p_size : ((p_size + p_alignment - 1) / p_alignment) * p_alignment;
}

using HashCode = uint32_t;
using UUID = uint32_t;

/**
 * @brief Hash a string in compile time.
 *
 * @param p_str The string to hash.
 * @return The hash code of the string.
 */
consteval HashCode static_hash(const char* p_str) {
    // from https://stackoverflow.com/questions/2111667/compile-time-string-hashing
    return *p_str ? static_cast<uint32_t>(*p_str) + 33 * static_hash(p_str + 1) : 5381;
}

/**
 * @brief Hash a string.
 *
 * @param p_str The string to hash.
 * @return The hash code of the string.
 */
constexpr HashCode dynam_hash(const char* p_str) {
    // from https://stackoverflow.com/questions/2111667/compile-time-string-hashing
    return *p_str ? static_cast<uint32_t>(*p_str) + 33 * dynam_hash(p_str + 1) : 5381;
}

/**
 * @brief Hash a string.
 *
 * @param p_str The string to hash.
 * @return The hash code of the string.
 */
constexpr HashCode dynam_hash(const std::string& p_str) {
    return dynam_hash(p_str.c_str());
}

using TypeID = HashCode;
using ConstID = HashCode;

template <typename T>
struct TypeIDTrait;

template <TypeID T>
struct TypeIDRevTrait;

template <HashCode CODE>
struct LabelTrait;

/**
 * @brief Split the string with respect to a token.
 *
 * @param p_str The string to split.
 * @param p_token The token.
 * @return Array of strings.
 */
inline std::vector<std::string> split_string(std::string p_str, char p_token) {
    std::vector<std::string> result;
    auto split_pos = p_str.find(p_token);
    while (split_pos != std::string::npos) {
        result.push_back(p_str.substr(0, split_pos));
        p_str.erase(0, split_pos + 1);
        split_pos = p_str.find(p_token);
    }
    result.push_back(p_str);
    return result;
}

/**
 * @brief Get the position that any of the tokens is first found in the string.
 *
 * @param p_str The string to find the token.
 * @param p_tokens The tokens to look for.
 * @return The index of the token that is first found.
 */
inline size_t find_first_pos(const std::string& p_str, const std::string& p_tokens) {
    if (p_tokens.size() == 0) {
        return std::string::npos;
    }
    size_t min_pos = std::numeric_limits<size_t>::infinity();
    for (char c : p_tokens) {
        min_pos = std::min(min_pos, p_str.find(c));
    }
    return min_pos;
}

/**
 * @brief Split the string with respect to a list of tokens.
 *
 * @todo Test
 * @param p_str The string to split.
 * @param p_token The list of tokens.
 * @return Array of strings.
 */
inline std::vector<std::string> split_string(std::string p_str, const std::string& p_tokens) {
    std::vector<std::string> result;
    auto split_pos = find_first_pos(p_str, p_tokens);
    while (split_pos != std::string::npos) {
        result.push_back(p_str.substr(0, split_pos));
        p_str.erase(0, split_pos);
        split_pos = find_first_pos(p_str, p_tokens);
    }
    return result;
}

/**
 * @brief Get an encode type from a string.
 *
 * @param p_str The string to get the encode type.
 * @return The encode type.
 */
inline EncodeType get_encode_type_from_str(const std::string& p_str) {
    if (p_str == "utf8") {
        return EncodeType::UTF8;
    }
    if (p_str == "utf16le") {
        return EncodeType::UTF16LE;
    }
    if (p_str == "utf16be") {
        return EncodeType::UTF16BE;
    }
    if (p_str == "utf32le") {
        return EncodeType::UTF32LE;
    }
    if (p_str == "utf32be") {
        return EncodeType::UTF32BE;
    }
    return EncodeType::UNKNOWN;
}

/**
 * @brief Load a text file from a path.
 * @todo Test
 *
 * @param p_path The path to the text file.
 * @return The content of the text file.
 */
inline std::string load_text_file(const char* p_path, EncodeType p_encode_type = EncodeType::UTF8) {
    if (p_encode_type != EncodeType::UTF8) {
        return ""; // TODO
    }
    std::string content;
    std::ifstream file_stream(p_path, std::ios::in);
    if (!file_stream.is_open()) {
        throw std::runtime_error("Failed to open text file at path: " + std::string(p_path));
    }
    std::string line = "";
    while (!file_stream.eof()) {
        std::getline(file_stream, line);
        content.append(line + "\n");
    }
    return content;
}

/**
 * @brief Load a binary file from a path.
 *
 * @todo Test
 * @param p_path The path tot he binary file.
 * @return The content of the binary file.
 */
inline std::vector<char> load_binary_file(const char* p_path) {
    std::ifstream file_stream(p_path, std::ios::binary);
    if (!file_stream.is_open()) {
        throw std::runtime_error("Failed to open file at path: " + std::string(p_path));
    }
    file_stream.seekg(0, std::ios::end);
    size_t size = file_stream.tellg();
    file_stream.seekg(0, std::ios::beg);
    std::vector<char> content(size);
    if (!file_stream.read(content.data(), size)) {
        throw std::runtime_error("Failed to read file at path: " + std::string(p_path));
    }
    return content;
}

/**
 * @brief Remove hashtag comments.
 * @todo Test
 *
 * @param str The string to remove comment.
 * @return The string that all the hashtag comments that are all removed.
 */
inline std::string remove_hashtag_comments(std::string str) {
    while (str.find('#') != std::string::npos) {
        size_t pos = str.find('#');
        size_t end = str.find('\n', pos);
        str.erase(pos, end - pos);
    }
    return str;
}

/**
 * @class EngineConfigOptions
 * @brief Engine configurations.
 *
 */
struct EngineConfigOptions {
    /**
     * @brief Name of the engine.
     */
    const char* engine_name = "White Bird Engine";
    /**
     * @brief Engine version major.
     */
    const uint32_t version_major = 0;
    /**
     * @brief Engine version minor.
     */
    const uint32_t version_minor = 0;
    /**
     * @brief Engine version patch.
     */
    const uint32_t version_patch = 1;

    /**
     * @brief The size of the tick stack.
     */
    size_t single_tick_stack_size = WBE_KiB(64);
    /**
     * @brief The size of the global memory pool.
     */
    size_t global_mem_pool_size = WBE_KiB(128);
    /**
     * @brief The size of the thread memory pool.
     */
    size_t thread_mem_pool_size = WBE_KiB(16);

    /**
     * @brief The utility name while running the program.
     */
    std::string utility_name;
};

/**
 * @class GameConfigOptions
 * @brief Configuration options for game.
 *
 */
struct GameConfigOptions {
    /**
     * @brief The name of the game.
     */
    std::string game_name = "White Bird Engine Demo";
    /**
     * @brief Game version major.
     */
    uint32_t version_major = 1;
    /**
     * @brief Game version minor.
     */
    uint32_t version_minor = 0;
    /**
     * @brief Game version patch.
     */
    uint32_t version_patch = 0;
    /**
     * @brief The width of the window.
     */
    uint32_t window_width = 1920;
    /**
     * @brief The height of the window.
     */
    uint32_t window_height = 1080;
    /**
     * @brief The index of the render device.
     * -1 if not specified, and white bird engine will automatically
     *  choose the most optimal one.
     */
    int32_t render_device_index = -1;

    /**
     * @brief Render API
     */
    enum class RenderAPI {
        // Dummy renderer
        DUMMY,
        // Vulkan render API
        VULKAN
    } render_api = RenderAPI::VULKAN;

    /**
     * @brief Set limit to the rendering fps. -1 stands for unlimited.
     */
    int32_t limit_render_fps = -1;

    /**
     * @brief Set limit to the logic fps. -1 stands for unlimited.
     * Unlimited logic FPS is strongly unrecognized, since logic computation will consume
     * substancial amount of CPU resources.
     */
    int32_t limit_logic_fps = 60;
};

// Color space
enum class ColorSpace {
    // RGB space
    RGB,
    // SRGB space
    SRGB
};

using ChannelID = HashCode;

template <typename T>
inline T required(const std::string& p_value_name, std::optional<T> p_optional_value) {
    if (!p_optional_value.has_value()) {
        throw std::runtime_error("Option \"" + p_value_name + "\" is required.");
    }
    return p_optional_value.value();
}

template <typename T>
inline std::optional<T>& requires_valid(const std::string& p_value_name, std::optional<T>& p_optional_value) {
    if (!p_optional_value.has_value()) {
        throw std::runtime_error("Option \"" + p_value_name + "\" is required.");
    }
    return p_optional_value;
}

template <typename T>
inline std::optional<T> requires_valid(const std::string& p_value_name, std::optional<T>&& p_optional_value) {
    if (!p_optional_value.has_value()) {
        throw std::runtime_error("Option \"" + p_value_name + "\" is required.");
    }
    return p_optional_value;
}

template <typename T>
inline T* requires_valid(const std::string& p_value_name, T* p_optional_value) {
    if (p_optional_value == nullptr) {
        throw std::runtime_error("Option \"" + p_value_name + "\" is required.");
    }
    return p_optional_value;
}

template <typename T>
inline std::shared_ptr<T> requires_valid(const std::string& p_value_name, std::shared_ptr<T> p_optional_value) {
    if (p_optional_value == nullptr) {
        throw std::runtime_error("Option \"" + p_value_name + "\" is required.");
    }
    return p_optional_value;
}

template <typename T>
inline std::unique_ptr<T> requires_valid(const std::string& p_value_name, std::unique_ptr<T> p_optional_value) {
    if (p_optional_value == nullptr) {
        throw std::runtime_error("Option \"" + p_value_name + "\" is required.");
    }
    return p_optional_value;
}

/**
 * @class BufferBase
 * @brief Base class for buffers. Used to identify buffers.
 *
 */
template <typename Child>
struct BufferBase {
    // The size of the buffer (in bytes).
    static constexpr size_t BUFFER_SIZE = Child::BUFFER_SIZE;
};

/**
 * @brief A buffer type.
 */
template <size_t SIZE>
struct Buffer : public BufferBase<Buffer<SIZE>> {
    // The size of the buffer (in bytes).
    static constexpr size_t BUFFER_SIZE = SIZE;
    // The buffer.
    char buffer[SIZE];
};

WBE_DECL_CRTP_CONCEPT(BufferBase);

}
#endif

