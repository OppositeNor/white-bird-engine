file(GLOB wbe_global_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(WBE_LIB_GLOBAL STATIC
    ${wbe_global_src}
)

set(libs ${libs}
    WBE_LIB_GLOBAL
    PARENT_SCOPE
)

set(WBE_BUILD_CONFIG_SCRIPTS ${WBE_BUILD_CONFIG_SCRIPTS}
    PARENT_SCOPE
)

