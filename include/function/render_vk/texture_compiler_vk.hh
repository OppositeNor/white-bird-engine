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
#ifndef WBE_FILE_TEXTURE_COMPILER_VK_HH
#define WBE_FILE_TEXTURE_COMPILER_VK_HH

#include "core/memory/reference_raw.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/global_render_cache.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/render_context_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <string_view>

namespace WhiteBirdEngine {

/**
 * @class TextureCompilerVK
 * @brief Uploads texture resources into sampled Vulkan images, shared through the GlobalRenderCache. Copyable and movable.
 *
 * Holds non-owning pointers to the render data and the cache, which must outlive it. Runs on the rendering thread.
 */
class TextureCompilerVK final {
public:
    TextureCompilerVK() = delete;
    ~TextureCompilerVK() = default;
    TextureCompilerVK(const TextureCompilerVK&) = default;
    TextureCompilerVK(TextureCompilerVK&&) noexcept = default;
    TextureCompilerVK& operator=(const TextureCompilerVK&) = default;
    TextureCompilerVK& operator=(TextureCompilerVK&&) noexcept = default;

    /**
     * @brief Construct a compiler.
     *
     * @throws std::runtime_error If the render data or the cache is null.
     */
    TextureCompilerVK(const RenderContextVK* p_render_data, GlobalRenderCache* p_render_cache);

    /**
     * @brief Get the cached view of a texture, uploading the texture and creating its default view on a miss.
     *
     * @param p_resource_key Cache key: the image is cached under it and the view under `<p_resource_key>#image_view`, which
     * also name them.
     * @param p_texture_resource The texture to upload.
     * @return The view, covering every mip level and layer of the image.
     * @throws std::runtime_error If the texture is null or libktx Vulkan upload is unavailable.
     */
    Ref<ROImageViewVK> compile_texture_view(std::string_view p_resource_key, RefRes<RUTexture> p_texture_resource) const;

    /**
     * @brief Get the cached image of a texture, uploading it on a miss.
     *
     * @param p_resource_key Cache key of the image, which also names it.
     * @param p_texture_resource The texture to upload.
     * @throws std::runtime_error If the texture is null or libktx Vulkan upload is unavailable.
     */
    Ref<ROImageVK> compile_texture(std::string_view p_resource_key, RefRes<RUTexture> p_texture_resource) const;

private:
    const RenderContextVK* render_data;
    GlobalRenderCache* render_cache;
};

} // namespace WhiteBirdEngine

#endif
