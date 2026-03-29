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
#ifndef WBE_FILE_RESOURCE_UNIT_LOADER_HH
#define WBE_FILE_RESOURCE_UNIT_LOADER_HH

#include "resource/resource_chunk/i_resource_chunk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/defs.hh"
#include <any>

namespace WhiteBirdEngine {

/**
 * @class IResourceUnitLoader
 * @brief Dynamic loader interface for resource units.
 */
class IResourceUnitLoader {
public:
    IResourceUnitLoader() = default;
    virtual ~IResourceUnitLoader() = default;
    WBE_R6_NDCD_DEFAULT_COPY_MOVE(IResourceUnitLoader)

    /**
     * @brief Checks whether this loader supports the input scheme.
     */
    virtual bool is_resource_supported(const RUScheme& p_scheme) const = 0;

    /**
     * @brief Populates a typed scheme into the input chunk.
     *
     * The implementation should dynamic-cast p_scheme to the expected derived
     * resource scheme type and throw if the cast fails.
     */
    virtual void populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const = 0;

    /**
     * @brief Populates a parser node into the input chunk.
     *
     * This is used by dynamic chunk loading to deserialize then populate via
     * populate_scheme.
     */
    virtual void populate_scheme(IResourceChunk& p_res_chunk, const std::any& p_scheme_data) const = 0;
};

class ResourceUnitLoaderAudio final : public IResourceUnitLoader {
public:
    virtual bool is_resource_supported(const RUScheme& p_scheme) const override;
    virtual void populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const override;
    virtual void populate_scheme(IResourceChunk& p_res_chunk, const std::any& p_scheme_data) const override;
};

class ResourceUnitLoaderBinary final : public IResourceUnitLoader {
public:
    virtual bool is_resource_supported(const RUScheme& p_scheme) const override;
    virtual void populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const override;
    virtual void populate_scheme(IResourceChunk& p_res_chunk, const std::any& p_scheme_data) const override;
};

class ResourceUnitLoaderShader final : public IResourceUnitLoader {
public:
    virtual bool is_resource_supported(const RUScheme& p_scheme) const override;
    virtual void populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const override;
    virtual void populate_scheme(IResourceChunk& p_res_chunk, const std::any& p_scheme_data) const override;
};

class ResourceUnitLoaderPipeline final : public IResourceUnitLoader {
public:
    virtual bool is_resource_supported(const RUScheme& p_scheme) const override;
    virtual void populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const override;
    virtual void populate_scheme(IResourceChunk& p_res_chunk, const std::any& p_scheme_data) const override;
};

class ResourceUnitLoaderMaterial final : public IResourceUnitLoader {
public:
    virtual bool is_resource_supported(const RUScheme& p_scheme) const override;
    virtual void populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const override;
    virtual void populate_scheme(IResourceChunk& p_res_chunk, const std::any& p_scheme_data) const override;
};

class ResourceUnitLoaderFont final : public IResourceUnitLoader {
public:
    virtual bool is_resource_supported(const RUScheme& p_scheme) const override;
    virtual void populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const override;
    virtual void populate_scheme(IResourceChunk& p_res_chunk, const std::any& p_scheme_data) const override;
};

class ResourceUnitLoaderImage final : public IResourceUnitLoader {
public:
    virtual bool is_resource_supported(const RUScheme& p_scheme) const override;
    virtual void populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const override;
    virtual void populate_scheme(IResourceChunk& p_res_chunk, const std::any& p_scheme_data) const override;
};

class ResourceUnitLoaderMesh final : public IResourceUnitLoader {
public:
    virtual bool is_resource_supported(const RUScheme& p_scheme) const override;
    virtual void populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const override;
    virtual void populate_scheme(IResourceChunk& p_res_chunk, const std::any& p_scheme_data) const override;
};

class ResourceUnitLoaderSubmesh final : public IResourceUnitLoader {
public:
    virtual bool is_resource_supported(const RUScheme& p_scheme) const override;
    virtual void populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const override;
    virtual void populate_scheme(IResourceChunk& p_res_chunk, const std::any& p_scheme_data) const override;
};

class ResourceUnitLoaderText final : public IResourceUnitLoader {
public:
    virtual bool is_resource_supported(const RUScheme& p_scheme) const override;
    virtual void populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const override;
    virtual void populate_scheme(IResourceChunk& p_res_chunk, const std::any& p_scheme_data) const override;
};

class ResourceUnitLoaderVideo final : public IResourceUnitLoader {
public:
    virtual bool is_resource_supported(const RUScheme& p_scheme) const override;
    virtual void populate_scheme(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const override;
    virtual void populate_scheme(IResourceChunk& p_res_chunk, const std::any& p_scheme_data) const override;
};

} // namespace WhiteBirdEngine

#endif