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
#ifndef WBE_FILE_RENDER_VK_RESOURCE_UNIT_LOADER_HH
#define WBE_FILE_RENDER_VK_RESOURCE_UNIT_LOADER_HH

#include "core/parser/parser_json.hh"
#include "resource/resource_chunk/i_resource_chunk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "resource/resource_unit_loader/resource_unit_loader.hh"
#include <any>
#include <span>
#include <string>

namespace WhiteBirdEngine {

// TODO: Test
class ResourceUnitLoaderShader final : public IResourceUnitLoader {
public:
    virtual std::span<const std::string> get_supported_resource_types() const override;
    virtual void populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const override;

protected:
    virtual std::any populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const override;
};

// TODO: Test
class ResourceUnitLoaderGraphicsPipeline final : public IResourceUnitLoader {
public:
    virtual std::span<const std::string> get_supported_resource_types() const override;
    virtual void populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const override;

protected:
    virtual std::any populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const override;
};

// TODO: Test
class ResourceUnitLoaderMaterial final : public IResourceUnitLoader {
public:
    virtual std::span<const std::string> get_supported_resource_types() const override;
    virtual void populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const override;

protected:
    virtual std::any populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const override;

private:
    ResourceUnitLoaderImage image_loader;
};

class ResourceUnitLoaderMesh final : public IResourceUnitLoader {
public:
    virtual std::span<const std::string> get_supported_resource_types() const override;
    virtual void populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const override;

protected:
    virtual std::any populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const override;
};

class ResourceUnitLoaderSubmesh final : public IResourceUnitLoader {
public:
    virtual std::span<const std::string> get_supported_resource_types() const override;
    virtual void populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const override;

protected:
    virtual std::any populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const override;
};

class ResourceUnitLoaderStaticOpaqueSet final : public IResourceUnitLoader {
public:
    virtual std::span<const std::string> get_supported_resource_types() const override;
    virtual void populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const override;

protected:
    virtual std::any populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const override;
};

class ResourceUnitLoaderStaticMaskedSet final : public IResourceUnitLoader {
public:
    virtual std::span<const std::string> get_supported_resource_types() const override;
    virtual void populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const override;

protected:
    virtual std::any populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const override;
};

void register_renderer_vk_resource_unit_loaders();

} // namespace WhiteBirdEngine

#endif
