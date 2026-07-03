#ifndef WBE_FILE_RENDER_STAGE_SOFTWARE
#define WBE_FILE_RENDER_STAGE_SOFTWARE

namespace WhiteBirdEngine {

/**
 * @class RenderStageSoftware
 * @brief A single render stage.
 */
class RenderStageSoftware {
public:
    RenderStageSoftware() = default;
    virtual ~RenderStageSoftware() = default;
    RenderStageSoftware(const RenderStageSoftware&) = delete;
    RenderStageSoftware(RenderStageSoftware&&) = delete;
    RenderStageSoftware& operator=(const RenderStageSoftware&) = delete;
    RenderStageSoftware& operator=(RenderStageSoftware&&) = delete;

    /**
     * @brief Execute the pipeline state.
     */
    virtual void execute() = 0;
};

} // namespace WhiteBirdEngine

#endif
