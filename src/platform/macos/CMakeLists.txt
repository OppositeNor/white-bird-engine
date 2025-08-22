# Copyright (c) 2025 OppositeNor.
# Licensed under GPL-3.0
file(GLOB wbe_macos_platform_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

set(wbe_platform_src
    ${wbe_platform_src}
    ${wbe_macos_platform_src}
    PARENT_SCOPE
)

