# Copyright (c) 2025 OppositeNor.
# Licensed under GPL-3.0
file(GLOB wbe_resource_unit_loader_png_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

set(wbe_resource_src
    ${wbe_resource_src}
    ${wbe_resource_unit_loader_png_src}
    PARENT_SCOPE
)

