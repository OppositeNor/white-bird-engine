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
#include "resource/resource_chunk/resource_chunk_load_file.hh"
#include <stdexcept>

namespace WhiteBirdEngine {
ResourceChunkLoadFile::RUType ResourceChunkLoadFile::get_res_type(const std::string& p_type) {
    if (p_type.starts_with("audio.")) {
        return RUType::AUDIO;
    }
    if (p_type.starts_with("binary.")) {
        return RUType::BINARY;
    }
    if (p_type.starts_with("font.")) {
        return RUType::FONT;
    }
    if (p_type.starts_with("image.")) {
        return RUType::IMAGE;
    }
    if (p_type.starts_with("text.")) {
        return RUType::TEXT;
    }
    if (p_type.starts_with("video.")) {
        return RUType::VIDEO;
    }
    throw std::runtime_error("Failed to get resource type. Type: " + p_type + " not recognized.");
}
}
