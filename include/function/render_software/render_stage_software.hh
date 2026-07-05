#ifndef WBE_FILE_RENDER_STAGE_SOFTWARE
#define WBE_FILE_RENDER_STAGE_SOFTWARE

#include "core/allocator/stl_allocator.hh"
#include "function/render_software/render_software_defs.hh"
namespace WhiteBirdEngine {

class RenderStagesSoftware final {
public:
    using RenderStageType = void(*)(BaseRenderDataSoftware*);

    struct Builder {
        Builder& set_vertex_shading_stage(RenderStageType p_stage) {
            vertex_shading_stage = p_stage;
            return *this;
        }
        Builder& set_rasterization_stage(RenderStageType p_stage) {
            rasterization_stage = p_stage;
            return *this;
        }
        Builder& set_fragment_shading_stage(RenderStageType p_stage) {
            fragment_shading_stage = p_stage;
            return *this;
        }
        RenderStageType vertex_shading_stage = default_vertex_shading_stage;
        RenderStageType rasterization_stage = default_rasterization_stage;
        RenderStageType fragment_shading_stage = default_fragment_shading_stage;
    };

    RenderStagesSoftware();
    ~RenderStagesSoftware() = default;
    RenderStagesSoftware(const RenderStagesSoftware&) = delete;
    RenderStagesSoftware(RenderStagesSoftware&&) = delete;
    RenderStagesSoftware& operator=(const RenderStagesSoftware&) = delete;
    RenderStagesSoftware& operator=(RenderStagesSoftware&&) = delete;

    Vector<RenderStageType> get_graphics_pipeline(BaseRenderDataSoftware* p_render_data) const {
        return Vector<RenderStageType>({
                vertex_shading_stage,
                rasterization_stage,
                fragment_shading_stage},
                p_render_data->main_allocator);
    }

private:
    static void default_vertex_shading_stage(BaseRenderDataSoftware* p_render_data) noexcept;
    static void default_rasterization_stage(BaseRenderDataSoftware* p_render_data) noexcept;
    static void default_fragment_shading_stage(BaseRenderDataSoftware* p_render_data) noexcept;

    RenderStageType vertex_shading_stage;
    RenderStageType rasterization_stage;
    RenderStageType fragment_shading_stage;
};

} // namespace WhiteBirdEngine

#endif
