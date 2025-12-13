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
#ifndef __WBE_RESOURCE_UNIT_POPULATOR_INTERFACE_HH__
#define __WBE_RESOURCE_UNIT_POPULATOR_INTERFACE_HH__

#include "resource/resource_chunk/resource_chunk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/defs.hh"
namespace WhiteBirdEngine {
/**
 * @class RUPopulatorInterface
 * @brief Populator interface. Populates the resource from its scheme to the
 * resource chunk.
 *
 */
class RUPopulatorInterface {
public:
    RUPopulatorInterface() = default;
    virtual ~RUPopulatorInterface() = default;
    WBE_R6_NDCD_DEFAULT_COPY_MOVE(RUPopulatorInterface)

    /**
     * @brief Populate the audio resource.
     *
     * @param p_res_chunk The resource chunk to insert resource data.
     * @param p_scheme The resource scheme.
     */
    virtual void populate(ResourceChunk& p_res_chunk, const RUSchemeAudio& p_scheme) const = 0;

    /**
     * @brief Populate the binary resource.
     *
     * @param p_res_chunk The resource chunk to insert resource data.
     * @param p_scheme The resource scheme.
     */
    virtual void populate(ResourceChunk& p_res_chunk, const RUSchemeBinary& p_scheme) const = 0;

    /**
     * @brief Populate the font resource.
     *
     * @param p_res_chunk The resource chunk to insert resource data.
     * @param p_scheme The resource scheme.
     */
    virtual void populate(ResourceChunk& p_res_chunk, const RUSchemeFont& p_scheme) const = 0;

    /**
     * @brief Populate the image resource.
     *
     * @param p_res_chunk The resource chunk to insert resource data.
     * @param p_scheme The resource scheme.
     */
    virtual void populate(ResourceChunk& p_res_chunk, const RUSchemeImage& p_scheme) const = 0;

    /**
     * @brief Populate the material resource.
     *
     * @param p_res_chunk The resource chunk to insert resource data.
     * @param p_scheme The resource scheme.
     */
    virtual void populate(ResourceChunk& p_res_chunk, const RUSchemeMaterial& p_scheme) const = 0;

    /**
     * @brief Populate the mesh resource.
     *
     * @param p_res_chunk The resource chunk to insert resource data.
     * @param p_scheme The resource scheme.
     */
    virtual void populate(ResourceChunk& p_res_chunk, const RUSchemeMesh& p_scheme) const = 0;

    /**
     * @brief Populate the submesh 3d resource.
     *
     * @param p_res_chunk The resource chunk to insert resource data.
     * @param p_scheme The resource scheme.
     */
    virtual void populate(ResourceChunk& p_res_chunk, const RUSchemeSubmesh3D& p_scheme) const = 0;

    /**
     * @brief Populate the submesh 3d resource.
     *
     * @param p_res_chunk The resource chunk to insert resource data.
     * @param p_scheme The resource scheme.
     */
    virtual void populate(ResourceChunk& p_res_chunk, const RUSchemeText& p_scheme) const = 0;

    /**
     * @brief Populate the submesh 3d resource.
     *
     * @param p_res_chunk The resource chunk to insert resource data.
     * @param p_scheme The resource scheme.
     */
    virtual void populate(ResourceChunk& p_res_chunk, const RUSchemeVideo& p_scheme) const = 0;
};

} // namespace WhiteBirdEngine


#endif
