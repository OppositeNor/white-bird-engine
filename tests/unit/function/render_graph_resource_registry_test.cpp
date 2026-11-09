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
#include "core/data/data_value.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "fake_render_graph.hh"
#include "function/render/i_render_graph.hh"
#include "function/render/i_render_object.hh"
#include "function/render/i_resource_description.hh"
#include "function/render/i_resource_registrator.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_graph_resource_registry.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace WhiteBirdEngine {

namespace {

class RegistryTestObject final : public IRenderObject {
public:
    explicit RegistryTestObject(int64_t p_value) : value(p_value) {
    }
    virtual ~RegistryTestObject() override = default;
    RegistryTestObject(const RegistryTestObject&) = delete;
    RegistryTestObject(RegistryTestObject&&) noexcept = delete;
    RegistryTestObject& operator=(const RegistryTestObject&) = delete;
    RegistryTestObject& operator=(RegistryTestObject&&) noexcept = delete;

    virtual TypeID get_type_uuid() const override {
        return static_hash("RegistryTestObject");
    }

    int64_t value;

protected:
    virtual void apply_debug_name(std::string_view /*p_name*/) override {
    }
};

class RegistryOtherObject final : public IRenderObject {
public:
    RegistryOtherObject() = default;
    virtual ~RegistryOtherObject() override = default;
    RegistryOtherObject(const RegistryOtherObject&) = delete;
    RegistryOtherObject(RegistryOtherObject&&) noexcept = delete;
    RegistryOtherObject& operator=(const RegistryOtherObject&) = delete;
    RegistryOtherObject& operator=(RegistryOtherObject&&) noexcept = delete;

    virtual TypeID get_type_uuid() const override {
        return static_hash("RegistryOtherObject");
    }

protected:
    virtual void apply_debug_name(std::string_view /*p_name*/) override {
    }
};

// A description of one integer, or of the integer and a dependency key the registrator registers first.
class RegistryTestDescription final : public IResourceDescription {
public:
    static constexpr TypeID TYPE_ID = static_hash("RegistryTestDescription");

    explicit RegistryTestDescription(int64_t p_value, std::string p_dependency = {})
        : value(p_value), dependency(std::move(p_dependency)) {
    }
    virtual ~RegistryTestDescription() override = default;
    RegistryTestDescription(const RegistryTestDescription&) = default;
    RegistryTestDescription(RegistryTestDescription&&) noexcept = default;
    RegistryTestDescription& operator=(const RegistryTestDescription&) = delete;
    RegistryTestDescription& operator=(RegistryTestDescription&&) noexcept = delete;

    virtual TypeID get_type_uuid() const override {
        return TYPE_ID;
    }

    virtual bool equals(const IResourceDescription& p_other) const override {
        if (p_other.get_type_uuid() != TYPE_ID) {
            return false;
        }
        const auto& other = static_cast<const RegistryTestDescription&>(p_other);
        return other.value == value && other.dependency == dependency;
    }

    virtual Unique<IResourceDescription> clone(RenderAllocator* p_allocator) const override {
        return make_unique<RegistryTestDescription>(p_allocator, *this);
    }

    virtual DataValue serialize() const override {
        return DataValue::make_integer(value);
    }

    int64_t value;
    std::string dependency;
};

class RegistryOtherDescription final : public IResourceDescription {
public:
    RegistryOtherDescription() = default;
    virtual ~RegistryOtherDescription() override = default;
    RegistryOtherDescription(const RegistryOtherDescription&) = default;
    RegistryOtherDescription(RegistryOtherDescription&&) noexcept = default;
    RegistryOtherDescription& operator=(const RegistryOtherDescription&) = delete;
    RegistryOtherDescription& operator=(RegistryOtherDescription&&) noexcept = delete;

    virtual TypeID get_type_uuid() const override {
        return static_hash("RegistryOtherDescription");
    }

    virtual bool equals(const IResourceDescription& p_other) const override {
        return p_other.get_type_uuid() == get_type_uuid();
    }

    virtual Unique<IResourceDescription> clone(RenderAllocator* p_allocator) const override {
        return make_unique<RegistryOtherDescription>(p_allocator, *this);
    }

    virtual DataValue serialize() const override {
        return DataValue::make_map();
    }
};

class RegistryTestRegistrator final : public IResourceRegistrator {
public:
    RegistryTestRegistrator(RenderAllocator* p_allocator, size_t* p_compile_count)
        : allocator(p_allocator), compile_count(p_compile_count) {
    }
    virtual ~RegistryTestRegistrator() override = default;
    RegistryTestRegistrator(const RegistryTestRegistrator&) = delete;
    RegistryTestRegistrator(RegistryTestRegistrator&&) noexcept = delete;
    RegistryTestRegistrator& operator=(const RegistryTestRegistrator&) = delete;
    RegistryTestRegistrator& operator=(RegistryTestRegistrator&&) noexcept = delete;

    virtual TypeID get_description_type_uuid() const override {
        return RegistryTestDescription::TYPE_ID;
    }

    virtual Unique<IResourceDescription> deserialize_description(const DataValue& p_description, RenderAllocator* p_allocator) const override {
        if (p_description.get_kind() != DataValueKind::INTEGER) {
            throw std::runtime_error("The registry test description must be an integer.");
        }
        return make_unique<RegistryTestDescription>(p_allocator, p_description.as_integer());
    }

    virtual Ref<IRenderObject> compile(IRenderGraph& p_graph, std::string_view /*p_key*/, const IResourceDescription& p_description) override {
        const auto& description = static_cast<const RegistryTestDescription&>(p_description);
        if (!description.dependency.empty()) {
            p_graph.register_resource(description.dependency, RegistryTestDescription(description.value + 1));
        }
        ++*compile_count;
        return make_ref<RegistryTestObject>(allocator, description.value);
    }

private:
    RenderAllocator* allocator;
    size_t* compile_count;
};

class RegistryOtherRegistrator final : public IResourceRegistrator {
public:
    explicit RegistryOtherRegistrator(RenderAllocator* p_allocator) : allocator(p_allocator) {
    }
    virtual ~RegistryOtherRegistrator() override = default;
    RegistryOtherRegistrator(const RegistryOtherRegistrator&) = delete;
    RegistryOtherRegistrator(RegistryOtherRegistrator&&) noexcept = delete;
    RegistryOtherRegistrator& operator=(const RegistryOtherRegistrator&) = delete;
    RegistryOtherRegistrator& operator=(RegistryOtherRegistrator&&) noexcept = delete;

    virtual TypeID get_description_type_uuid() const override {
        return RegistryOtherDescription().get_type_uuid();
    }

    virtual Unique<IResourceDescription> deserialize_description(const DataValue& /*p_description*/, RenderAllocator* p_allocator) const override {
        return make_unique<RegistryOtherDescription>(p_allocator);
    }

    virtual Ref<IRenderObject> compile(IRenderGraph& /*p_graph*/, std::string_view /*p_key*/, const IResourceDescription& /*p_description*/) override {
        return make_ref<RegistryOtherObject>(allocator);
    }

private:
    RenderAllocator* allocator;
};

} // namespace

class WBERenderGraphResourceRegistryTest : public ::testing::Test {
protected:
    WBERenderGraphResourceRegistryTest() : graph(&allocator) {
        graph.add_resource_registrator(make_unique<RegistryTestRegistrator>(&allocator, &allocator, &compile_count));
    }

    RenderAllocator allocator{WBE_KI_B(64)};
    size_t compile_count = 0;
    FakeRenderGraph graph;
};

TEST_F(WBERenderGraphResourceRegistryTest, RegistersCompiledObjectsUnderTheirKey) {
    const Ref<IRenderObject> object = graph.register_resource("first", RegistryTestDescription(3));
    ASSERT_NE(object, MEM_NULL);
    EXPECT_TRUE(graph.has_resource_registrator(RegistryTestDescription::TYPE_ID));
    EXPECT_FALSE(graph.has_resource_registrator(RegistryOtherDescription().get_type_uuid()));
    EXPECT_TRUE(graph.contains_registered_resource("first"));
    EXPECT_EQ(graph.try_acquire_registered_resource("first"), object);
    EXPECT_EQ(graph.acquire_registered<RegistryTestObject>("first")->value, 3);
    EXPECT_EQ(graph.try_acquire_registered_resource("missing"), MEM_NULL);
    EXPECT_EQ(graph.try_acquire_registered<RegistryTestObject>("missing"), MEM_NULL);
    EXPECT_THROW((void)graph.acquire_registered_resource("missing"), std::runtime_error);
    EXPECT_THROW((void)graph.acquire_registered<RegistryTestObject>("missing"), std::runtime_error);
    EXPECT_THROW((void)graph.acquire_registered<RegistryOtherObject>("first"), std::runtime_error);
}

TEST_F(WBERenderGraphResourceRegistryTest, KeepsRegisteredObjectsAlive) {
    {
        (void)graph.register_resource("kept", RegistryTestDescription(1));
    }
    ASSERT_NE(graph.try_acquire_registered_resource("kept"), MEM_NULL);
    EXPECT_EQ(graph.acquire_registered<RegistryTestObject>("kept")->value, 1);
}

TEST_F(WBERenderGraphResourceRegistryTest, EqualDescriptionsReuseTheRegisteredObject) {
    const Ref<IRenderObject> first = graph.register_resource("key", RegistryTestDescription(5));
    const Ref<IRenderObject> second = graph.register_resource("key", RegistryTestDescription(5));
    EXPECT_EQ(first, second);
    EXPECT_EQ(compile_count, 1U);
}

TEST_F(WBERenderGraphResourceRegistryTest, ChangedDescriptionsReplaceTheRegisteredObject) {
    const Ref<IRenderObject> first = graph.register_resource("key", RegistryTestDescription(5));
    const Ref<IRenderObject> second = graph.register_resource("key", RegistryTestDescription(6));
    EXPECT_NE(first, second);
    EXPECT_EQ(compile_count, 2U);
    EXPECT_EQ(graph.acquire_registered<RegistryTestObject>("key")->value, 6);
}

TEST_F(WBERenderGraphResourceRegistryTest, ReleasesReplacedAndUnregisteredObjectsThroughTheCallback) {
    std::vector<Ref<IRenderObject>> released;
    FakeRenderGraph owner(&allocator);
    RenderGraphResourceRegistry registry(&allocator, &owner, [&released](Ref<IRenderObject> p_object) {
        released.push_back(std::move(p_object));
    });
    registry.add_registrator(make_unique<RegistryTestRegistrator>(&allocator, &allocator, &compile_count));
    const Ref<IRenderObject> first = registry.register_resource("key", RegistryTestDescription(1));
    const Ref<IRenderObject> second = registry.register_resource("key", RegistryTestDescription(2));
    ASSERT_EQ(released.size(), 1U);
    EXPECT_EQ(released[0], first);
    registry.unregister_resource("key");
    ASSERT_EQ(released.size(), 2U);
    EXPECT_EQ(released[1], second);
    EXPECT_FALSE(registry.contains("key"));
    EXPECT_THROW(registry.unregister_resource("key"), std::runtime_error);
}

TEST_F(WBERenderGraphResourceRegistryTest, RegistratorsMayRegisterOtherKeysWhileCompiling) {
    (void)graph.register_resource("dependent", RegistryTestDescription(10, "dependency"));
    EXPECT_EQ(graph.acquire_registered<RegistryTestObject>("dependent")->value, 10);
    EXPECT_EQ(graph.acquire_registered<RegistryTestObject>("dependency")->value, 11);
    EXPECT_EQ(compile_count, 2U);
}

TEST_F(WBERenderGraphResourceRegistryTest, DeserializesScriptDescriptionsWithTheRegistrator) {
    const Ref<IRenderObject> object = graph.register_resource("scripted", RegistryTestDescription::TYPE_ID, DataValue::make_integer(7));
    EXPECT_EQ(graph.acquire_registered<RegistryTestObject>("scripted")->value, 7);
    EXPECT_EQ(graph.register_resource("scripted", RegistryTestDescription::TYPE_ID, DataValue::make_integer(7)), object);
    EXPECT_EQ(compile_count, 1U);
    EXPECT_THROW((void)graph.register_resource("scripted", RegistryTestDescription::TYPE_ID, DataValue::make_string("seven")), std::runtime_error);
}

TEST_F(WBERenderGraphResourceRegistryTest, DispatchesOnTheDescriptionType) {
    graph.add_resource_registrator(make_unique<RegistryOtherRegistrator>(&allocator, &allocator));
    (void)graph.register_resource("key", RegistryTestDescription(1));
    EXPECT_NE(graph.try_acquire_registered<RegistryTestObject>("key"), MEM_NULL);
    // A description of another type replaces the registration with its own registrator's object.
    (void)graph.register_resource("key", RegistryOtherDescription());
    EXPECT_NE(graph.try_acquire_registered<RegistryOtherObject>("key"), MEM_NULL);
    EXPECT_THROW((void)graph.try_acquire_registered<RegistryTestObject>("key"), std::runtime_error);
    EXPECT_EQ(compile_count, 1U);
}

TEST_F(WBERenderGraphResourceRegistryTest, RejectsInvalidRegistrations) {
    // No registrator accepts RegistryOtherDescription.
    EXPECT_THROW((void)graph.register_resource("key", RegistryOtherDescription()), std::runtime_error);
    EXPECT_THROW((void)graph.register_resource("key", RegistryOtherDescription().get_type_uuid(), DataValue::make_map()), std::runtime_error);
    EXPECT_THROW((void)graph.register_resource("", RegistryTestDescription(1)), std::runtime_error);
    EXPECT_FALSE(graph.contains_registered_resource("key"));
    EXPECT_EQ(compile_count, 0U);
}

TEST_F(WBERenderGraphResourceRegistryTest, RejectsInvalidRegistrators) {
    // RegistryTestDescription already has a registrator.
    EXPECT_THROW(graph.add_resource_registrator(make_unique<RegistryTestRegistrator>(&allocator, &allocator, &compile_count)),
        std::runtime_error);
    EXPECT_THROW(graph.add_resource_registrator(Unique<IResourceRegistrator>()), std::runtime_error);
}

} // namespace WhiteBirdEngine
