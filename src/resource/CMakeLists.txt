file(GLOB wbe_resource_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_subdirectory(resource_manager)
add_subdirectory(resource_chunk)

add_library(WBE_LIB_RESOURCE STATIC
    ${wbe_resource_src}
)

set(libs ${libs}
    WBE_LIB_RESOURCE
    PARENT_SCOPE
)

set(WBE_BUILD_CONFIG_SCRIPTS ${WBE_BUILD_CONFIG_SCRIPTS}
    PARENT_SCOPE
)

