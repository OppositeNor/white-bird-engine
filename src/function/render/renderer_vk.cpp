#include "function/render/renderer_vk.hh"
#include "function/engine_function.hh"

#include <SDL_vulkan.h>

namespace WhiteBirdEngine {

RendererVK::RendererVK() {
    init();
}

RendererVK::~RendererVK() {
    terminate();
}

void RendererVK::init() {
    init_window();
    init_vulkan();
}

void RendererVK::terminate() {
    SDL_DestroyWindow(window);
    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();
}

void RendererVK::init_window() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Vulkan_LoadLibrary(nullptr);
    auto& game_config = EngineFunction::get_singleton()->game_config->get_config_options();
    window = SDL_CreateWindow(game_config.game_name.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              game_config.window_width, game_config.window_height, SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);
}

void RendererVK::init_vulkan() {
    create_instance();
    // TODO
}

void RendererVK::create_instance() {
    // TODO
}


}
