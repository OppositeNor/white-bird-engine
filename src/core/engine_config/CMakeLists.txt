file(GLOB engine_config_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(ENGINE_CONFIG_LIB STATIC
    ${engine_config_src}
)

set(libs ${libs}
    ENGINE_CONFIG_LIB
    PARENT_SCOPE
)


