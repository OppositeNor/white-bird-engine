file(GLOB wbe_function_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_subdirectory(render)

add_library(WBE_LIB_FUNCTION STATIC
    ${wbe_function_src}
)

set(libs ${libs}
    WBE_LIB_FUNCTION
    PARENT_SCOPE
)

set(WBE_BUILD_CONFIG_SCRIPTS ${WBE_BUILD_CONFIG_SCRIPTS}
    PARENT_SCOPE
)

