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
#ifndef WBE_FILE_RO_IMAGE_HH
#define WBE_FILE_RO_IMAGE_HH

#include "function/render/i_rend_object.hh"

namespace WhiteBirdEngine {

/**
 * @class ROImage
 * @brief Backend-independent image render object. Neither copyable nor movable.
 *
 * Confined to the rendering thread, like every render object.
 */
class ROImage : public IRendObject {
public:
    ROImage() = default;
    virtual ~ROImage() override = default;
    ROImage(const ROImage&) = delete;
    ROImage(ROImage&&) noexcept = delete;
    ROImage& operator=(const ROImage&) = delete;
    ROImage& operator=(ROImage&&) noexcept = delete;
};

} // namespace WhiteBirdEngine

#endif
