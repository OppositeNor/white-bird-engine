#ifndef __WBE_COMPONENTS_TEST_H__
#define __WBE_COMPONENTS_TEST_H__
#include <gtest/gtest.h>
#include "core/ecs/ecs.h"

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
    WBE::ComponentsChunk<MockComponent>mock_component;
    mock_component.add(mock_entity, 1, 3.5f, 3.1415926);
}


#endif

