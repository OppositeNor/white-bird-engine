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

#include "resource/resource_chunk/resource_chunk.hh"
#include "resource/resource_unit/resource_unit.hh"
namespace WhiteBirdEngine {
/**
 * @class RUPopulator
 * @brief Populator class. Populates the resource from its scheme to the
 * resource chunk.
 *
 */
class RUPopulator {
public:
    RUPopulator() = default;
    virtual ~RUPopulator() {}

    /**
     * @brief Populate the resource.
     *
     * @param p_res_chunk The resource chunk to insert resource data.
     * @param p_scheme The resource scheme.
     */
    virtual void populate(ResourceChunk& p_res_chunk, RUScheme& p_scheme) const = 0;
};

}


#endif
