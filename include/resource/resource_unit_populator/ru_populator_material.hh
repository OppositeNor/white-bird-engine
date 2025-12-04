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
#ifndef __WBE_RU_POPULATOR_MATERIAL_HH__
#define __WBE_RU_POPULATOR_MATERIAL_HH__

#include "resource_unit_populator.hh"

namespace WhiteBirdEngine {
/**
 * @class RUPopulatorMaterial
 * @brief RUMaterial populator.
 *
 */
class RUPopulatorMaterial final : public RUPopulator {
public:
    RUPopulatorMaterial() {}
    virtual ~RUPopulatorMaterial() override {}

    virtual void populate(ResourceChunk& p_res_chunk, RUScheme& p_scheme) const override {
        // TODO
    }
    
};


}

#endif
