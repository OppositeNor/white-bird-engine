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
#include "function/scene/scene_resource_unit_loader.hh"
#include "core/parser/parser_json.hh"
#include "resource/resource_chunk/i_resource_chunk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <any>
#include <array>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace WhiteBirdEngine {
namespace {

[[noreturn]] void throw_loader_not_implemented(std::string_view p_resource_type) {
    throw std::runtime_error(
        std::string("Resource unit loader is not implemented for scene resource type: ") + std::string(p_resource_type) + ".");
}

} // namespace

std::span<const std::string> ResourceUnitLoaderComponent::get_supported_resource_types() const {
    static const std::array<std::string, 1> supported_resource_types = {"component"};
    return supported_resource_types;
}

void ResourceUnitLoaderComponent::populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    (void)p_res_chunk;
    (void)p_scheme_data;
    throw_loader_not_implemented("component");
}

std::any ResourceUnitLoaderComponent::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    (void)p_res_chunk;
    (void)p_scheme;
    throw_loader_not_implemented("component");
}

std::span<const std::string> ResourceUnitLoaderPrefab::get_supported_resource_types() const {
    static const std::array<std::string, 1> supported_resource_types = {"prefab"};
    return supported_resource_types;
}

void ResourceUnitLoaderPrefab::populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    (void)p_res_chunk;
    (void)p_scheme_data;
    throw_loader_not_implemented("prefab");
}

std::any ResourceUnitLoaderPrefab::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    (void)p_res_chunk;
    (void)p_scheme;
    throw_loader_not_implemented("prefab");
}

std::span<const std::string> ResourceUnitLoaderWorld::get_supported_resource_types() const {
    static const std::array<std::string, 1> supported_resource_types = {"world"};
    return supported_resource_types;
}

void ResourceUnitLoaderWorld::populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    (void)p_res_chunk;
    (void)p_scheme_data;
    throw_loader_not_implemented("world");
}

std::any ResourceUnitLoaderWorld::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    (void)p_res_chunk;
    (void)p_scheme;
    throw_loader_not_implemented("world");
}

std::span<const std::string> ResourceUnitLoaderScene::get_supported_resource_types() const {
    static const std::array<std::string, 1> supported_resource_types = {"scene"};
    return supported_resource_types;
}

void ResourceUnitLoaderScene::populate_scheme_from_data(IResourceChunk& p_res_chunk, const ParserJSON::DataType& p_scheme_data) const {
    (void)p_res_chunk;
    (void)p_scheme_data;
    throw_loader_not_implemented("scene");
}

std::any ResourceUnitLoaderScene::populate_scheme_ptr(IResourceChunk& p_res_chunk, const RUScheme& p_scheme) const {
    (void)p_res_chunk;
    (void)p_scheme;
    throw_loader_not_implemented("scene");
}

} // namespace WhiteBirdEngine
