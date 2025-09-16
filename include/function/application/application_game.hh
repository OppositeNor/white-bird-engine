#ifndef __WBE_APPLICATION_GAME_HH__
#define __WBE_APPLICATION_GAME_HH__

#include "core/allocator/heap_allocator_aligned_pool.hh"
#include "core/memory/reference_strong.hh"
#include "function/application/application.hh"
#include "function/render/renderer.hh"
namespace WhiteBirdEngine {

class ApplicationGame : public Application {
public:
    ApplicationGame();
    virtual ~ApplicationGame() {}
    ApplicationGame(const ApplicationGame&) = delete;
    ApplicationGame(ApplicationGame&&) = delete;
    ApplicationGame& operator=(const ApplicationGame&) = delete;
    ApplicationGame& operator=(ApplicationGame&&) = delete;

    virtual void render_tick(double p_delta) override;
    virtual void logic_tick(double p_delta) override;

    /**
     * @brief Get the allocator for the renderer.
     *
     * @return The allocator for the renderer.
     */
    Ref<const HeapAllocatorAlignedPool> get_renderer_allocator() const {
        return renderer_allocator;
    }

private:
    Ref<HeapAllocatorAlignedPool> renderer_allocator;
    void init();
protected:
    Ref<Renderer> renderer;
};

}

#endif
