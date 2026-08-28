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
Texture2D<float2> shadow_moments_input;
RWTexture2D<float2> shadow_moments_horizontal_output;

/**
 * @brief Build the horizontal prefix sums for a shadow-moment summed-area table.
 * @param p_dispatch_thread_id The dispatched row index.
 */
[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 p_dispatch_thread_id : SV_DispatchThreadID) {
    uint width;
    uint height;
    shadow_moments_input.GetDimensions(width, height);
    uint row = p_dispatch_thread_id.x;
    if (row >= height) {
        return;
    }
    float2 moment_sum = float2(0.0);
    for (uint column = 0; column < width; ++column) {
        // H(x, y) = sum(M(i, y), i = 0..x).
        moment_sum += shadow_moments_input.Load(int3(column, row, 0));
        shadow_moments_horizontal_output[uint2(column, row)] = moment_sum;
    }
}
