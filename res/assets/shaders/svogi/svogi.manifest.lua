-- Copyright 2025 OppositeNor
--
-- Licensed under the Apache License, Version 2.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--     http://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.
return {
    {
        id = "svogi_occupancy_injection",
        source_type = "shader",
        src = "assets/shaders/svogi/svogi_kernel.slang",
        kernel = "inject_svo_occupancy",
    },
    {
        id = "svogi_radiance_injection",
        source_type = "shader",
        src = "assets/shaders/svogi/svogi_kernel.slang",
        kernel = "inject_svogi_radiance",
    },
    {
        id = "svogi_occupancy_propagation",
        source_type = "shader",
        src = "assets/shaders/svogi/svogi_kernel.slang",
        kernel = "propagate_svogi_occupancy",
    },
}
