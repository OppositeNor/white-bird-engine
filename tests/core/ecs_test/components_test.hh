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
#ifndef __WBE_COMPONENTS_TEST_HH__
#define __WBE_COMPONENTS_TEST_HH__
#include <gtest/gtest.h>
#include "core/ecs/ecs.hh"

namespace WBE = WhiteBirdEngine;

struct MockComponent {
    MockComponent(int p_val1, float p_val2, double p_val3)
        : val1(p_val1), val2(p_val2), val3(p_val3) {}
    
    int val1;
    float val2;
    double val3;
};

TEST(WBEECSTest, General) {
    WBE::EntityID mock_entity = WBE::static_hash("Mock entity");
    WBE::ComponentsChunk<MockComponent> mock_component;
    WBE::MemID component_id_1 = mock_component.add(mock_entity, 1, 3.5f, 3.1415926);
    WBE::MemID component_id_2 = mock_component.add(mock_entity, 2, 4.5f, 3.14);
    auto* obj_1 = mock_component.get_allocator().get_obj<MockComponent>(component_id_1);
    ASSERT_EQ(obj_1->val1, 1);
    ASSERT_FLOAT_EQ(obj_1->val2, 3.5f);
    ASSERT_DOUBLE_EQ(obj_1->val3, 3.1415926);
    auto* obj_2 = mock_component.get_allocator().get_obj<MockComponent>(component_id_2);
    ASSERT_EQ(obj_2->val1, 2);
    ASSERT_FLOAT_EQ(obj_2->val2, 4.5f);
    ASSERT_DOUBLE_EQ(obj_2->val3, 3.14);
    mock_component.clear();
}


#endif

