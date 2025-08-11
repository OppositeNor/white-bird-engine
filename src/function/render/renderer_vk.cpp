#include "function/render/renderer_vk.hh"
#include "core/engine_core.hh"
#include "function/engine_function.hh"

#include <SDL_vulkan.h>
#include <vulkan/vulkan_core.h>

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
    vkDestroyInstance(vk_instance, nullptr);
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
    auto& engine_config = EngineCore::get_singleton()->engine_config->get_config_options();
    auto& game_config = EngineFunction::get_singleton()->game_config->get_config_options();
    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = game_config.game_name.c_str();
    app_info.applicationVersion = VK_MAKE_VERSION(game_config.version_major, game_config.version_minor, game_config.version_patch);
    app_info.pEngineName = engine_config.engine_name;
    app_info.engineVersion = VK_MAKE_VERSION(engine_config.version_major, engine_config.version_minor, engine_config.version_patch);
    app_info.apiVersion = VK_API_VERSION_1_0;

    create_info.pApplicationInfo = &app_info;

    if constexpr (enable_validation_layer) {
        create_info.enabledLayerCount = validation_layers.size();
        create_info.ppEnabledLayerNames = validation_layers.data();
    }
    else {
        create_info.enabledLayerCount = 0;
        create_info.ppEnabledLayerNames = nullptr;
    }

    uint32_t extensions_count = 0;
    SDL_Vulkan_GetInstanceExtensions(window, &extensions_count, nullptr);
    std::vector<const char*> extension_names(extensions_count);
    SDL_Vulkan_GetInstanceExtensions(window, &extensions_count, extension_names.data());

    create_info.enabledExtensionCount = extensions_count;
    create_info.ppEnabledExtensionNames = extension_names.data();

    if (vkCreateInstance(&create_info, nullptr, &vk_instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create vulkan instance.");
    }
}

bool RendererVK::check_validation_layer_support() {
    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::vector<VkLayerProperties> layer_properties(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, layer_properties.data());
    for (auto* name : validation_layers) {
        bool layer_found = false;
        for (auto& layer_prop : layer_properties) {
            if (strcmp(layer_prop.layerName, name) == 0) {
                layer_found = true;
                break;
            }
        }

        if (!layer_found) {
            return false;
        }
    }
    return true;
}

}
