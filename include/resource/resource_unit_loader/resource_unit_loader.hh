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

#include "core/parser/parser_json.hh"
#include "resource/resource_chunk/i_resource_chunk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <any>
#include <span>
#include <string>

namespace WhiteBirdEngine {

/**
 * @class IResourceUnitLoader
 * @brief Dynamic loader interface for resource units.
 */
class IResourceUnitLoader {
public:
    IResourceUnitLoader() = default;
    virtual ~IResourceUnitLoader() = default;
    IResourceUnitLoader(const IResourceUnitLoader&) = default;
    IResourceUnitLoader(IResourceUnitLoader&&) noexcept = default;
    IResourceUnitLoader& operator=(const IResourceUnitLoader&) = default;
    IResourceUnitLoader& operator=(IResourceUnitLoader&&) noexcept = default;

    /**
     * @brief Gets all resource types supported by this loader.
     *
     * @return A read-only view of the supported resource type names. The view remains valid for the lifetime of the program.
     */
    virtual std::span<const std::string> get_supported_resource_types() const = 0;

    /**
     * @brief Populates a scheme into the input chunk.
     *
     * @param p_res_chunk The resource chunk to populate the scheme into.
     * @param p_scheme The scheme to populate.
     */
    void populate_scheme(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const {
        (void)populate_scheme_ptr(p_res_chunk, p_scheme);
    }

    /**
     * @brief Populates a parser node into the input chunk.
     *
     * @param p_res_chunk The resource chunk to populate the scheme into.
     * @param p_scheme_data The parser node to populate.
     */
    virtual void populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const = 0;

    /**
     * @brief Populates a scheme into the input chunk and returns a type-erased reference to the result.
     *
     * @param p_res_chunk The resource chunk to populate the scheme into.
     * @param p_scheme The scheme to populate.
     * @return A type-erased reference to the resource unit in the chunk.
     */
    virtual std::any populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const = 0;
};

class ResourceUnitLoaderAudio final : public IResourceUnitLoader {
public:
    virtual std::span<const std::string> get_supported_resource_types() const override;
    virtual void populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const override;

protected:
    virtual std::any populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const override;
};

class ResourceUnitLoaderBinary final : public IResourceUnitLoader {
public:
    virtual std::span<const std::string> get_supported_resource_types() const override;
    virtual void populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const override;

protected:
    virtual std::any populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const override;
};

class ResourceUnitLoaderFont final : public IResourceUnitLoader {
public:
    virtual std::span<const std::string> get_supported_resource_types() const override;
    virtual void populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const override;

protected:
    virtual std::any populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const override;
};

class ResourceUnitLoaderImage final : public IResourceUnitLoader {
public:
    virtual std::span<const std::string> get_supported_resource_types() const override;
    virtual void populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const override;

protected:
    virtual std::any populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const override;
};

class ResourceUnitLoaderText final : public IResourceUnitLoader {
public:
    virtual std::span<const std::string> get_supported_resource_types() const override;
    virtual void populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const override;

protected:
    virtual std::any populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const override;
};

class ResourceUnitLoaderVideo final : public IResourceUnitLoader {
public:
    virtual std::span<const std::string> get_supported_resource_types() const override;
    virtual void populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const override;

protected:
    virtual std::any populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUSchema& p_scheme) const override;
};

} // namespace WhiteBirdEngine

#endif
