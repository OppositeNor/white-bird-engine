add_subdirectory(allocator_test)
# add_subdirectory(ecs_test)
add_subdirectory(memory_test)
add_subdirectory(logging_test)
add_subdirectory(parser_test)
add_subdirectory(profiler_test)

set(WBE_BUILD_CONFIG_SCRIPTS ${WBE_BUILD_CONFIG_SCRIPTS}
    PARENT_SCOPE
)
