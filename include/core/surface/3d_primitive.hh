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
#ifndef WBE_FILE_3D_PRIMITIVE_HH
#define WBE_FILE_3D_PRIMITIVE_HH

namespace WhiteBirdEngine {

/**
 * @class Triangle3D
 * @brief 3D triangle.
 *
 * @tparam TVertType The type of vertices.
 */
template <typename TVertType>
struct Triangle3D {
    /**
     * @brief The first vertex of the triangle.
     */
    TVertType vert1;
    /**
     * @brief The second vertex of the triangle.
     */
    TVertType vert2;
    /**
     * @brief The third vertex of the triangle.
     */
    TVertType vert3;
};

} // namespace WhiteBirdEngine

#endif
