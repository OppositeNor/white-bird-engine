# Copyright (c) 2025 OppositeNor.
# Licensed under GPL-3.0
file(GLOB wbe_function_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_subdirectory(render)

add_library(WBE_LIB_FUNCTION STATIC
    ${wbe_function_src}
)

# TODO: also link other libraries
find_package(Vulkan REQUIRED)

target_link_libraries(WBE_LIB_FUNCTION PUBLIC
    Vulkan::Vulkan
    SDL2::SDL2-static
    yaml-cpp::yaml-cpp
)

set(libs ${libs}
    WBE_LIB_FUNCTION
    PARENT_SCOPE
)

set(WBE_BUILD_CONFIG_SCRIPTS ${WBE_BUILD_CONFIG_SCRIPTS}
    PARENT_SCOPE
)

