/* Copyright 2026 OppositeNor

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
#ifndef WBE_FILE_SHADER_DEBUG_HH
#define WBE_FILE_SHADER_DEBUG_HH

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace WhiteBirdEngine {

/** Number of shader error entries available per frame after its two-word header. */
inline constexpr size_t SHADER_DEBUG_FRAME_ENTRY_CAPACITY = 512;
/** Number of uint32 words in one frame's shader-debug region. */
inline constexpr size_t SHADER_DEBUG_FRAME_WORD_COUNT = SHADER_DEBUG_FRAME_ENTRY_CAPACITY + 2;
/** Number of bytes in one frame's shader-debug region. */
inline constexpr size_t SHADER_DEBUG_FRAME_BYTE_SIZE = SHADER_DEBUG_FRAME_WORD_COUNT * sizeof(uint32_t);
/** Shader-visible descriptor-heap index reserved for the shader-debug buffer. */
inline constexpr uint32_t SHADER_DEBUG_DESCRIPTOR_INDEX = 0;

/**
 * @enum ShaderDebugError
 * @brief Stable error codes reported by GPU shaders.
 */
enum class ShaderDebugError : uint32_t {
    /** The report buffer ran out of entry storage. */
    DEBUG_BUFFER_FULL = 1,
    /** An SVO node allocation would exceed its buffer. */
    SVO_NODE_INDEX_OVERFLOW = 2,
    /** An SVO leaf allocation would exceed its buffer. */
    SVO_LEAF_INDEX_OVERFLOW = 3,
    /** An SVO deferred task would exceed its ping-pong buffer. */
    SVO_DEFERRED_TASK_INDEX_OVERFLOW = 4,
    /** An SVO branch extends beyond its configured maximum depth. */
    SVO_DEEPER_THAN_EXPECTED = 5,
    /** An SVO branch terminates before its configured maximum depth. */
    SVO_SHALLOWER_THAN_EXPECTED = 6,
};

/**
 * @brief Return the stable name of a shader-debug error code.
 * @param p_error Error code reported by the GPU.
 * @return The enumerator name, or `UNKNOWN_SHADER_DEBUG_ERROR` for an unknown value.
 */
constexpr std::string_view shader_debug_error_name(ShaderDebugError p_error) {
    switch (p_error) {
    case ShaderDebugError::DEBUG_BUFFER_FULL:
        return "DEBUG_BUFFER_FULL";
    case ShaderDebugError::SVO_NODE_INDEX_OVERFLOW:
        return "SVO_NODE_INDEX_OVERFLOW";
    case ShaderDebugError::SVO_LEAF_INDEX_OVERFLOW:
        return "SVO_LEAF_INDEX_OVERFLOW";
    case ShaderDebugError::SVO_DEFERRED_TASK_INDEX_OVERFLOW:
        return "SVO_DEFERRED_TASK_INDEX_OVERFLOW";
    case ShaderDebugError::SVO_DEEPER_THAN_EXPECTED:
        return "SVO_DEEPER_THAN_EXPECTED";
    case ShaderDebugError::SVO_SHALLOWER_THAN_EXPECTED:
        return "SVO_SHALLOWER_THAN_EXPECTED";
    }
    return "UNKNOWN_SHADER_DEBUG_ERROR";
}

} // namespace WhiteBirdEngine

#endif
