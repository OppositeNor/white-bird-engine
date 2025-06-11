add_subdirectory(allocator)
add_subdirectory(engine_config)
add_subdirectory(global)
add_subdirectory(logging)
add_subdirectory(profiling)

set(libs ${libs}
    PARENT_SCOPE
)

