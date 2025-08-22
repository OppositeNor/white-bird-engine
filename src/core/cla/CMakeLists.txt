# Copyright (c) 2025 OppositeNor.
# Licensed under GPL-3.0
file(GLOB wbe_cla_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

set(wbe_core_src
    ${wbe_core_src}
    ${wbe_cla_src}
    PARENT_SCOPE
)

