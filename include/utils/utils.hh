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
#ifndef WBE_FILE_UTILS_HH
#define WBE_FILE_UTILS_HH

#include "utils/defs.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <ios>
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

// Type of access.
enum class AccessType : uint8_t {
    // Read only.
    READ_ONLY = 0b01,
    // Write only.
    WRITE_ONLY = 0b10,
    // Read or write.
    READ_WRITE = 0b11
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
using TypeID = uint32_t;

/**
 * @brief Hash a string in compile time.
 *
 * @param p_str The string to hash.
 * @return The hash code of the string.
 */
consteval HashCode static_hash(const char* p_str) {
    // from
    // https://stackoverflow.com/questions/2111667/compile-time-string-hashing
    return *p_str ? static_cast<uint32_t>(*p_str) + 33 * static_hash(p_str + 1) : 5381;
}

/**
 * @brief Hash a string.
 *
 * @param p_str The string to hash.
 * @return The hash code of the string.
 */
constexpr HashCode dynam_hash(const char* p_str) {
    // from
    // https://stackoverflow.com/questions/2111667/compile-time-string-hashing
    return *p_str ? static_cast<uint32_t>(*p_str) + 33 * dynam_hash(p_str + 1) : 5381;
}

/**
 * @brief Hash a int32_t in compile time.
 *
 * @param p_val The value to hash.
 * @return The hash code of the value.
 */
consteval HashCode static_hash(int32_t p_val) {
    // from
    // https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key#12996028
    p_val = ((p_val >> 16) ^ p_val) * 0x45d9f3b;
    p_val = ((p_val >> 16) ^ p_val) * 0x45d9f3b;
    p_val = (p_val >> 16) ^ p_val;
    return p_val;
}

/**
 * @brief Hash a int32_t.
 *
 * @param p_val The value to hash.
 * @return The hash code of the value.
 */
constexpr HashCode dynam_hash(int32_t p_val) {
    // from
    // https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key#12996028
    p_val = ((p_val >> 16) ^ p_val) * 0x45d9f3b;
    p_val = ((p_val >> 16) ^ p_val) * 0x45d9f3b;
    p_val = (p_val >> 16) ^ p_val;
    return p_val;
}

/**
 * @brief Hash a uint32_t in compile time.
 *
 * @param p_val The value to hash.
 * @return The hash code of the value.
 */
consteval HashCode static_hash(uint32_t p_val) {
    // from
    // https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key#12996028
    p_val = ((p_val >> 16) ^ p_val) * 0x45d9f3bU;
    p_val = ((p_val >> 16) ^ p_val) * 0x45d9f3bU;
    p_val = (p_val >> 16) ^ p_val;
    return p_val;
}

/**
 * @brief Hash a uint32_t.
 *
 * @param p_val The value to hash.
 * @return The hash code of the value.
 */
constexpr HashCode dynam_hash(uint32_t p_val) {
    // from
    // https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key#12996028
    p_val = ((p_val >> 16) ^ p_val) * 0x45d9f3bU;
    p_val = ((p_val >> 16) ^ p_val) * 0x45d9f3bU;
    p_val = (p_val >> 16) ^ p_val;
    return p_val;
}

/**
 * @brief Hash a int64_t in compile time.
 *
 * @param p_val The value to hash.
 * @return The hash code of the value.
 */
consteval HashCode static_hash(int64_t p_val) {
    // from
    // https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key#12996028
    p_val = (p_val ^ (p_val >> 30)) * 0xbf58476d1ce4e5b9LL; // NOLINT
    p_val = (p_val ^ (p_val >> 27)) * 0x94d049bb133111ebLL; // NOLINT
    p_val = p_val ^ (p_val >> 31);
    return p_val;
}

/**
 * @brief Hash a int64_t.
 *
 * @param p_val The value to hash.
 * @return The hash code of the value.
 */
constexpr HashCode dynam_hash(int64_t p_val) {
    // from
    // https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key#12996028
    p_val = (p_val ^ (p_val >> 30)) * 0xbf58476d1ce4e5b9LL; // NOLINT
    p_val = (p_val ^ (p_val >> 27)) * 0x94d049bb133111ebLL; // NOLINT
    p_val = p_val ^ (p_val >> 31);
    return p_val;
}

/**
 * @brief Hash a uint64_t in compile time.
 *
 * @param p_val The value to hash.
 * @return The hash code of the value.
 */
consteval HashCode static_hash(uint64_t p_val) {
    // from
    // https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key#12996028
    p_val = (p_val ^ (p_val >> 30)) * 0xbf58476d1ce4e5b9ULL;
    p_val = (p_val ^ (p_val >> 27)) * 0x94d049bb133111ebULL;
    p_val = p_val ^ (p_val >> 31);
    return p_val;
}

/**
 * @brief Hash a uint64_t.
 *
 * @param p_val The value to hash.
 * @return The hash code of the value.
 */
constexpr HashCode dynam_hash(uint64_t p_val) {
    // from
    // https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key#12996028
    p_val = (p_val ^ (p_val >> 30)) * 0xbf58476d1ce4e5b9ULL;
    p_val = (p_val ^ (p_val >> 27)) * 0x94d049bb133111ebULL;
    p_val = p_val ^ (p_val >> 31);
    return p_val;
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
    if (p_tokens.empty()) {
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
    file_stream.seekg(0, std::ios::end);
    size_t size = file_stream.tellg();
    file_stream.seekg(0, std::ios::beg);
    content.resize(size);
    if (!file_stream.read(content.data(), static_cast<std::streamsize>(size))) {
        throw std::runtime_error("Failed to read text file at path: " + std::string(p_path));
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
    if (!file_stream.read(content.data(), static_cast<std::streamsize>(size))) {
        throw std::runtime_error("Failed to read file at path: " + std::string(p_path));
    }
    return content;
}

/**
 * @brief Load a binary file from a path.
 *
 * @todo Test
 * @param p_path The path tot he binary file.
 * @param p_result The result data. Set to nullptr if only interested with file
 * size.
 * @param p_size The size of the file. Ignored if p_result is not nullptr.
 */
inline void load_binary_file(const char* p_path, void* p_result, size_t* p_size) {
    if (p_path == nullptr) {
        throw std::invalid_argument("Path to binary file cannot be null.");
    }
    if (p_result == nullptr && p_size == nullptr) {
        throw std::invalid_argument("Either p_result or p_size must be provided.");
    }

    std::ifstream file_stream(p_path, std::ios::binary | std::ios::ate);
    if (!file_stream.is_open()) {
        throw std::runtime_error("Failed to open file at path: " + std::string(p_path));
    }

    const std::ifstream::pos_type end_pos = file_stream.tellg();
    if (end_pos < 0) {
        throw std::runtime_error("Failed to get file size at path: " + std::string(p_path));
    }

    const size_t size = static_cast<size_t>(end_pos);

    // Query mode: caller only needs file size.
    if (p_result == nullptr) {
        *p_size = size;
        return;
    }

    if (size > static_cast<size_t>(std::numeric_limits<std::streamsize>::max())) {
        throw std::runtime_error("File is too large to read at path: " + std::string(p_path));
    }

    file_stream.seekg(0, std::ios::beg);
    if (!file_stream.good()) {
        throw std::runtime_error("Failed to seek file at path: " + std::string(p_path));
    }

    if (size == 0) {
        return;
    }

    if (!file_stream.read(static_cast<char*>(p_result), static_cast<std::streamsize>(size))) {
        throw std::runtime_error("Failed to read file at path: " + std::string(p_path));
    }
}

/**
 * @brief Remove hashtag comments.
 * @todo Test
 *
 * @param str The string to remove comment.
 * @return The string that all the hashtag comments that are all removed.
 */
inline std::string remove_hashtag_comments(std::string p_str) {
    while (p_str.contains('#')) {
        size_t pos = p_str.find('#');
        size_t end = p_str.find('\n', pos);
        p_str.erase(pos, end - pos);
    }
    return p_str;
}

/**
 * @brief Hash a pointer in compile time. (Usually won't be used, since pointers
 * are usually dynamic).
 *
 * @param p_ptr The pointer to be hashed.
 * @return The hash code of the pointer.
 */
consteval HashCode static_hash_ptr(const void* p_ptr) {
    return static_hash(reinterpret_cast<uintptr_t>(p_ptr));
}

/**
 * @brief Hash a pointer in run time.
 *
 * @param p_ptr The pointer to be hashed.
 * @return The hash code of the pointer.
 */
inline HashCode dynam_hash_ptr(const void* p_ptr) {
    return dynam_hash(reinterpret_cast<uintptr_t>(p_ptr));
}

using ChannelID = HashCode;

// Color space
enum class ColorSpace {
    // RGB space
    RGB,
    // SRGB space
    SRGB
};

/**
 * @brief A better name for the required field.
 *
 * @tparam T The type of the required value.
 */
template <typename T>
using Required = std::optional<T>; // What's optional is actually required...

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

/**
 * @brief Increment an index on a ring.
 *
 * @param p_i The index to increment.
 * @param p_ring_size The size of the ring.
 * @return The incremented index.
 */
constexpr size_t ring_increment(size_t p_i, size_t p_ring_size) {
    return (p_i + 1) % p_ring_size;
}

/**
 * @brief Decrement an index on a ring.
 *
 * @param p_i The index to decrement.
 * @param p_ring_size The size of the ring.
 * @return The decremented index.
 */
constexpr size_t ring_decrement(size_t p_i, size_t p_ring_size) {
    return ((p_i + p_ring_size) - 1) % p_ring_size;
}

WBE_DECL_CRTP_CONCEPT(BufferBase);

} // namespace WhiteBirdEngine
#endif
