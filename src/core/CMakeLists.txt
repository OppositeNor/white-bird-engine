set(wbe_core_src "")

add_subdirectory(allocator)
add_subdirectory(global)
add_subdirectory(logging)
add_subdirectory(profiling)

add_library(WBE_LIB_CORE STATIC
    ${wbe_core_src}
)

set(libs ${libs}
    WBE_LIB_CORE
    PARENT_SCOPE
)

set(WBE_BUILD_CONFIG_SCRIPTS ${WBE_BUILD_CONFIG_SCRIPTS}
    PARENT_SCOPE
)
