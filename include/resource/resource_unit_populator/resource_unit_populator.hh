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
#ifndef __WBE_RESOURCE_UNIT_POPULATOR_HH__
#define __WBE_RESOURCE_UNIT_POPULATOR_HH__

#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "resource_unit_populator_interface.hh"

namespace WhiteBirdEngine {

/**
 * @class RUPopulator
 * @brief The implementation of the resource unit populator.
 * @todo Test
 */
class RUPopulator final : public RUPopulatorInterface {
public:
    RUPopulator();
    WBE_R6_NDC_DEFAULT_OVERRIDE(RUPopulator)

    virtual void populate(ResourceChunk& p_res_chunk, const RUSchemeAudio& p_scheme) const override;
    virtual void populate(ResourceChunk& p_res_chunk, const RUSchemeBinary& p_scheme) const override;
    virtual void populate(ResourceChunk& p_res_chunk, const RUSchemeFont& p_scheme) const override;
    virtual void populate(ResourceChunk& p_res_chunk, const RUSchemeImage& p_scheme) const override;
    virtual void populate(ResourceChunk& p_res_chunk, const RUSchemeMaterial& p_scheme) const override;
    virtual void populate(ResourceChunk& p_res_chunk, const RUSchemeMesh3D& p_scheme) const override;
    virtual void populate(ResourceChunk& p_res_chunk, const RUSchemeSubmesh3D& p_scheme) const override;
    virtual void populate(ResourceChunk& p_res_chunk, const RUSchemeText& p_scheme) const override;
    virtual void populate(ResourceChunk& p_res_chunk, const RUSchemeVideo& p_scheme) const override;

private:
    ResourceManager* resource_manager;

    void resolve_image_data(ResourceChunk& p_res_chunk, const RUSchemeImage& p_scheme, RUImage& p_resource_unit) const;
    void resolve_hdr_image_data(ResourceChunk& p_res_chunk, const RUSchemeImage& p_scheme, RUImage& p_resource_unit) const;
    void resolve_submesh_data(ResourceChunk& p_res_chunk, const RUSchemeSubmesh3D& p_scheme, RUSubmesh3D& p_resource_unit) const;
    RefRes<RUMaterial> resolve_material(ResourceChunk& p_res_chunk, const RUSchemeMaterial& p_material) const;
};

} // namespace WhiteBirdEngine

#endif
