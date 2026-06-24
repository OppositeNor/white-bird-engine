#ifndef WBE_FILE_RO_IMAGE_SOFTWARE
#define WBE_FILE_RO_IMAGE_SOFTWARE

#include "core/allocator/stl_allocator.hh"
#include "function/render/i_render_object.hh"
#include "function/render/render_defs.hh"
#include "glm/ext/vector_float4.hpp"
#include "glm/ext/vector_uint2.hpp"
#include <cstdint>
namespace WhiteBirdEngine {

/**
 * @class
 * @brief Image
 */
class ROImageSoftware : public IRenderObject {
public:
    ROImageSoftware() = delete;
    virtual ~ROImageSoftware() override = default;
    ROImageSoftware(const ROImageSoftware&) = delete;
    ROImageSoftware(ROImageSoftware&&) = delete;
    ROImageSoftware& operator=(const ROImageSoftware&) = delete;
    ROImageSoftware& operator=(ROImageSoftware&&) = delete;

    enum Format {
        R8G8B8A8_UNORM,
        R32_SFLOAT
    };

    ROImageSoftware(glm::uvec2 p_extent, uint32_t p_mipmap_lod = 1);

    void write_pixel(glm::uvec2 p_pos, const glm::vec4& p_data, uint32_t p_mip_level) {
        // TODO
    }

    uint32_t get_row_pitch(uint32_t p_mip_level = 0) const noexcept {
        return mip_infos[p_mip_level].row_pitch;
    }

    uint8_t* get_buffer_data() noexcept {
        return buffer.data();
    }

    const uint8_t* get_buffer_data() const noexcept {
        return buffer.data();
    }

private:

    struct MipInfo {
        uint32_t start_index;
        uint32_t p_width;
        uint32_t p_height;
        uint32_t row_pitch;
    };

    glm::uvec2 extent;
    uint32_t mipmap_lod;
    Vector<uint8_t, RenderAllocator> buffer;
    Vector<MipInfo> mip_infos;

    template <typename T>
    T& get_pixel_at_lod() {
        // TODO
    }
};

} // namespace WhiteBirdEngine

#endif
