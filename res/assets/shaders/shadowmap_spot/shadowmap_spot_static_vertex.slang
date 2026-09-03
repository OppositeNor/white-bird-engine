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
#include "utils.slang"

struct ShadowVSOutput {
    float4 position : SV_Position;
};

ConstantBuffer<CameraInfo> light_camera_info;
StructuredBuffer<NodeData> static_nodes;
StructuredBuffer<InstanceData> static_instances;

/**
 * @brief Transform a static-set vertex into spot-light clip space.
 * @param p_input Incoming vertex attributes.
 * @param p_instance_id Index into static_instances.
 * @return Clip-space vertex output.
 */
[shader("vertex")]
ShadowVSOutput vertex_main(Vertex3DTexturedInput p_input, uint p_instance_id : SV_VulkanInstanceID) {
    InstanceData instance = static_instances[p_instance_id];
    float4 world_position = mul(static_nodes[instance.node_index].transform, float4(p_input.position.xyz, 1.0));
    ShadowVSOutput output;
    output.position = mul(light_camera_info.proj, mul(light_camera_info.view, world_position));
    return output;
}
