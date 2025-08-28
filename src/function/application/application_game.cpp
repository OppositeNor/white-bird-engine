#include "function/application/application_game.hh"
#include "core/allocator/heap_allocator_aligned_pool.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/allocator_vk.hh"
#include "function/render/renderer_vk.hh"
#include "global/global.hh"

namespace WhiteBirdEngine {

ApplicationGame::ApplicationGame() {
    init();
}

void ApplicationGame::init() {
    renderer_allocator = make_ref<HeapAllocatorAlignedPool>(global_allocator(), WBE_MiB(1));
    renderer = make_ref<RendererVK>(global_allocator(), make_ref<AllocatorVK>(global_allocator(), renderer_allocator.get()));
    // TODO
}

void ApplicationGame::render_tick(double p_delta) {
    // TODO
}

void ApplicationGame::logic_tick(double p_delta) {
    // TODO
}

}
