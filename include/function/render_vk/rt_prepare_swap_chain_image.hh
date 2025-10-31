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
#ifndef __WBE_PREPARE_SWAP_CHAIN_IMAGE_HH__
#define __WBE_PREPARE_SWAP_CHAIN_IMAGE_HH__

#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_utils.hh"
namespace WhiteBirdEngine {

/**
 * @class RTPrepareSwapChianImage
 * @brief A dummy render task to hold the semaphore for swap chain image preparing.
 *
 */
struct RTPrepareSwapChianImage final: public RenderTaskVK {
    RTPrepareSwapChianImage(const BaseRenderDataVK* p_render_data)
        : RenderTaskVK(p_render_data) {}

    virtual void perform(double p_delta) override {}

};

/**
 * @class RTRunGraphicsPipelineFactory
 * @brief Factory for RTRunGraphicsPipeline.
 *
 */
class RTPrepareSwapChianImageFactory final : public RenderTaskFactoryVK {
public:
    /**
     * @brief Constructor.
     *
     * @param p_render_data The render data.
     * @param p_signal_in_flight Should signal in flight fence.
     */
    RTPrepareSwapChianImageFactory(const BaseRenderDataVK* p_render_data)
        : RenderTaskFactoryVK(p_render_data) {}
    virtual ~RTPrepareSwapChianImageFactory() override {}

    virtual Ref<RenderTask> create() const override {
        return make_ref<RTPrepareSwapChianImage>(render_data->allocator, render_data);      
    }

};
}

#endif
