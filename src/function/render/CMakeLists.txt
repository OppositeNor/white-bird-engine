# Copyright (c) 2025 OppositeNor.
# Licensed under GPL-3.0
file(GLOB wbe_render_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

set(wbe_function_src
    ${wbe_function_src}
    ${wbe_render_src}
    PARENT_SCOPE
)


