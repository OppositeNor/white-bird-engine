#ifndef __WBE_2D_SURFACE_UTILS_H__
#define __WBE_2D_SURFACE_UTILS_H__

#include "2d_primitive.h"

namespace WhiteBirdEngine {

/**
 * @brief Triangulate a list of vertices.
 *
 * @tparam VertSlot The type of the vertex slot.
 * @tparam TriaSlot The type of the triangle slot.
 * @param p_vertex_list The list of the vertices.
 * @param p_vertex_count The number of vertices to be triangulated.
 * @param p_triangle_list The list of triangles. The generated triangles will be pushed into this list.
 */
template <typename VertSlot, typename TriaSlot>
void triangulate(const VertSlot* p_vertex_list, uint32_t p_vertex_count, TriaSlot* p_triangle_list) {
    // TODO: Ear clipping
}

}

#endif
