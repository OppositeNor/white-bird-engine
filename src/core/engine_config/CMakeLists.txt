file(GLOB wbe_engine_config_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

set(wbe_core_src
    ${wbe_core_src}
    ${wbe_engine_config_src}
    PARENT_SCOPE
)

