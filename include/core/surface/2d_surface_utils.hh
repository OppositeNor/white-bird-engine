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
#ifndef __WBE_2D_SURFACE_UTILS_HH__
#define __WBE_2D_SURFACE_UTILS_HH__

#include "utils/defs.hh"

#include <glm/glm.hpp>
#include <cstdint>
#include <stdexcept>

namespace WhiteBirdEngine {

/**
 * @class SurfaceUtils2D
 * @brief 2D surface utility class.
 *
 */
class SurfaceUtils2D {
public:
    SurfaceUtils2D() = delete;

    /**
     * @brief Triangulate a list of vertices.
     *
     * @todo Test
     * @tparam VertType The type of the vertex slot.
     * @tparam TriaType The type of the triangle slot.
     * @param p_vertex_list The list of the vertices. This will be altered by this method.
     * @param p_vertex_count The number of vertices to be triangulated.
     * @param p_triangle_list The list of triangles. The generated triangles will be pushed into this list.
     */
    template <typename VertType, typename TriaType>
    static void triangulate(VertType* p_vertex_list, uint32_t p_vertex_count, TriaType* p_triangle_list) {
        if (p_vertex_list == nullptr) {
            throw std::runtime_error("Failed to triangulate: vertex list is null.");
        }
        if (p_triangle_list == nullptr) {
            throw std::runtime_error("Failed to triangulate: triangle list is null.");
        }
        uint32_t ear_check_index = 0;
        uint32_t triangle_index = 0;
        while (p_vertex_count > 3) {
            glm::vec2 prev_to_this = get_vert(p_vertex_list, p_vertex_count, ear_check_index)->position
                - get_vert(p_vertex_list, p_vertex_count, ear_check_index - 1)->position;
            glm::vec2 this_to_next = get_vert(p_vertex_list, p_vertex_count, ear_check_index + 1)->position
                - get_vert(p_vertex_list, p_vertex_count, ear_check_index)->position;
            float cross_prev_next = cross_vec2(prev_to_this, this_to_next);
            if (std::abs(cross_prev_next - 0.0) < WBE_FLOAT_TOLARENCE || is_vertex_ear(p_vertex_list, p_vertex_count, ear_check_index)) {
                p_triangle_list[triangle_index].vert1 = get_vert(p_vertex_list, p_vertex_count, ear_check_index - 1);
                p_triangle_list[triangle_index].vert2 = get_vert(p_vertex_list, p_vertex_count, ear_check_index);
                p_triangle_list[triangle_index].vert3 = get_vert(p_vertex_list, p_vertex_count, ear_check_index + 1);
                remove_vertex(p_vertex_list, p_vertex_count, ear_check_index);
                ++triangle_index;
                --p_vertex_count;
            }
            else {
                ear_check_index = (ear_check_index + 1) % p_vertex_count;
            }
        }
        p_triangle_list[triangle_index].vert1 = get_vert(p_vertex_list, p_vertex_count, 0);
        p_triangle_list[triangle_index].vert2 = get_vert(p_vertex_list, p_vertex_count, 1);
        p_triangle_list[triangle_index].vert3 = get_vert(p_vertex_list, p_vertex_count, 2);
    }

private:

    static float cross_vec2(glm::vec2 p_vec1, glm::vec2 p_vec2) {
        return glm::cross(glm::vec3(p_vec1.x, p_vec1.y, 0.0), glm::vec3(p_vec2.x, p_vec2.y, 0)).z;
    }

    template <typename VertType>
    static void remove_vertex(VertType* p_vert_list, uint32_t p_vert_count, uint32_t p_index) {
        for (uint32_t i = p_index; i < p_vert_count; ++i) {
            p_vert_list[i] = p_vert_list[i + 1];
        }
    }

    template <typename VertType>
    static VertType* get_vert(VertType* p_vertex_list, uint32_t p_vert_count, uint32_t p_index) {
        return &(p_vertex_list[p_index % p_vert_count]);
    }

    template <typename VertType>
    static bool is_vertex_ear(const VertType* p_vertex_list, uint32_t p_vert_count, uint32_t p_index) {
        for (uint32_t i = p_index + 2; i != (p_index + p_vert_count - 2) % p_vert_count; i = (i + 1) % p_vert_count) {
            glm::vec2 prev_to_this = get_vert(p_vertex_list, p_vert_count, i)->position
                - get_vert(p_vertex_list, p_vert_count, i - 1)->position;
            glm::vec2 this_to_next = get_vert(p_vertex_list, p_vert_count, i + 1)->position
                - get_vert(p_vertex_list, p_vert_count, i)->position;
            glm::vec2 i_pos = get_vert(p_vertex_list, p_vert_count, i)->position;
            glm::vec2 vert_next_pos = get_vert(p_vertex_list, p_vert_count, p_index + 1)->position;
            glm::vec2 vert_prev_pos = get_vert(p_vertex_list, p_vert_count, p_index - 1)->position;
            glm::vec2 vert_pos = get_vert(p_vertex_list, p_vert_count, p_index)->position;
            if (cross_vec2(i_pos - vert_prev_pos, vert_next_pos - vert_pos) < 0.0f
                && cross_vec2(i_pos - vert_next_pos, vert_pos - i_pos) < 0.0f
                && cross_vec2(i_pos - vert_pos, vert_prev_pos - i_pos) < 0.0f)
                return false;
        }
        return true;
    }
};



}

#endif
