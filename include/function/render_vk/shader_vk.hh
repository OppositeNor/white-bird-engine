#ifndef __WBE_SHADER_VK_HH__
#define __WBE_SHADER_VK_HH__

#include "function/render_vk/renderer_vk.hh"
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

class ShaderVK {
public:
    ShaderVK() = default;
    virtual ~ShaderVK() {}
    
protected:
    VkShaderModule create_shader_module(const RendererVK::BaseRenderDataVK* p_base_render_data, const char* p_code_data, size_t p_code_size);
};

}

#endif
