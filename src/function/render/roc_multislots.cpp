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
#include "function/render/roc_multislots.hh"
#include "core/alloc/i_alloc.hh"
#include "core/alloc/stl_alloc.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/i_rend_object.hh"
#include "function/render/rend_defs.hh"
#include "function/render/debug_data.hh"
#include "function/render/ro_buffer.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <utility>

namespace WhiteBirdEngine {

namespace {

Vector<Ref<IRendObject>, RendAlloc> create_slot_resources(RendAlloc* p_alloc, uint32_t p_slot_count) {
    if (p_alloc == nullptr) {
        throw std::runtime_error("ROCMultislots: allocator is null.");
    }
    if (p_slot_count == 0) {
        throw std::runtime_error("ROCMultislots: slot count is zero.");
    }

    Vector<Ref<IRendObject>, RendAlloc> resources(p_alloc);
    resources.reserve(p_slot_count);
    for (uint32_t slot = 0; slot < p_slot_count; ++slot) {
        resources.emplace_back(MEM_NULL);
    }
    return resources;
}

Vector<uint64_t, RendAlloc> create_applied_generations(RendAlloc* p_alloc, uint32_t p_slot_count) {
    Vector<uint64_t, RendAlloc> result(p_alloc);
    result.resize(p_slot_count, 0);
    return result;
}

Ref<ROBuffer> require_buffer_resource(Ref<IRendObject> p_resource, uint32_t p_slot) {
    if (p_resource == MEM_NULL) {
        throw std::runtime_error(std::format("ROCMultislots: cannot apply update to empty slot {}.", p_slot));
    }
    Ref<ROBuffer> buffer = p_resource.dynamic_cast_ref<ROBuffer>();
    if (buffer == MEM_NULL) {
        throw std::runtime_error(std::format("ROCMultislots: slot {} does not contain a buffer for its pending update.", p_slot));
    }
    return buffer;
}

} // namespace

ROCMultislots::ROCMultislots(RendAlloc* p_alloc, uint32_t p_slot_count, Factory p_factory, bool p_flush)
    : alloc(p_alloc), factory(std::move(p_factory)), flush(p_flush),
      resources(create_slot_resources(p_alloc, p_slot_count)),
      applied_generations(create_applied_generations(p_alloc, p_slot_count)), pending_buffer_data(p_alloc) {
    if (factory == nullptr) {
        throw std::runtime_error("ROCMultislots: resource factory is null.");
    }
    recreate();
}

void ROCMultislots::recreate() {
    for (uint32_t slot = 0; slot < resources.size(); ++slot) {
        Ref<IRendObject> resource_ref = factory(slot);
        if (resource_ref == MEM_NULL) {
            throw std::runtime_error(std::format("ROCMultislots: resource factory returned null for slot {}.", slot));
        }
        retain_resource_type(resource_ref);
        resources[slot] = std::move(resource_ref);
        // A new slot receives the newest published update again when it is next resolved.
        applied_generations[slot] = 0;
    }
}

Ref<IRendObject> ROCMultislots::get() {
    return get(0);
}

Ref<IRendObject> ROCMultislots::get() const {
    return get(0);
}

Ref<IRendObject> ROCMultislots::get(uint32_t p_slot) {
    if (p_slot >= resources.size()) {
        throw std::runtime_error(std::format("ROCMultislots: slot {} is out of range for {} slots.", p_slot, resources.size()));
    }
    synchronize(p_slot);
    return resources[p_slot];
}

Ref<IRendObject> ROCMultislots::get(uint32_t p_slot) const {
    if (p_slot >= resources.size()) {
        throw std::runtime_error(std::format("ROCMultislots: slot {} is out of range for {} slots.", p_slot, resources.size()));
    }
    synchronize(p_slot);
    return resources[p_slot];
}

DebugDataRows ROCMultislots::desc_debug_data(RendAlloc* p_alloc) const {
    DebugDataRows rows(p_alloc);
    for (uint32_t slot = 0; slot < resources.size(); ++slot) {
        if (resources[slot] == MEM_NULL) {
            continue;
        }
        DebugDataRows resource_rows = resources[slot]->desc_debug_data(p_alloc);
        for (DebugData& row : resource_rows) {
            row.set_value("slot_index", slot);
            rows.emplace_back(std::move(row));
        }
    }
    return rows;
}

void ROCMultislots::set(uint32_t p_slot, Ref<IRendObject> p_resource) {
    if (p_slot >= resources.size()) {
        throw std::runtime_error(std::format("ROCMultislots: slot {} is out of range for {} slots.", p_slot, resources.size()));
    }
    retain_resource_type(p_resource);
    resources[p_slot] = std::move(p_resource);
    if (flush) {
        applied_generations[p_slot] = 0;
    }
}

void ROCMultislots::clear(uint32_t p_slot) {
    if (p_slot >= resources.size()) {
        throw std::runtime_error(std::format("ROCMultislots: slot {} is out of range for {} slots.", p_slot, resources.size()));
    }
    resources[p_slot] = MEM_NULL;
    if (flush) {
        applied_generations[p_slot] = 0;
    }
}

void ROCMultislots::clear() {
    for (uint32_t slot = 0; slot < resources.size(); ++slot) {
        clear(slot);
    }
}

uint32_t ROCMultislots::get_slot_count() const {
    return static_cast<uint32_t>(resources.size());
}

void ROCMultislots::publish_buffer_update(const void* p_data, size_t p_dst_offset, size_t p_size) {
    if (p_data == nullptr || p_size == 0) {
        throw std::runtime_error("ROCMultislots: published buffer data is empty.");
    }
    const auto* data = static_cast<const uint8_t*>(p_data);
    if (!flush) {
        for (uint32_t slot = 0; slot < resources.size(); ++slot) {
            if (resources[slot] == MEM_NULL) {
                continue;
            }
            Ref<ROBuffer> buffer = require_buffer_resource(resources[slot], slot);
            buffer->update_buffer(data, p_dst_offset, p_size);
        }
        return;
    }
    pending_buffer_data.assign(data, data + p_size);
    pending_buffer_dst_offset = p_dst_offset;
    ++update_generation;
    if (update_generation == 0) {
        ++update_generation;
        std::fill(applied_generations.begin(), applied_generations.end(), 0);
    }
}

void ROCMultislots::synchronize(uint32_t p_slot) const {
    if (!flush) {
        return;
    }
    if (pending_buffer_data.empty() || applied_generations[p_slot] == update_generation) {
        return;
    }
    Ref<ROBuffer> buffer = require_buffer_resource(resources[p_slot], p_slot);
    buffer->update_buffer(pending_buffer_data.data(), pending_buffer_dst_offset, pending_buffer_data.size());
    applied_generations[p_slot] = update_generation;
}

} // namespace WhiteBirdEngine
