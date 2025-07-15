add_subdirectory(resource_chunk_test)
add_subdirectory(resource_loader_test)
add_subdirectory(resource_utils_test)

set(WBE_BUILD_CONFIG_SCRIPTS ${WBE_BUILD_CONFIG_SCRIPTS}
    PARENT_SCOPE
)

set(WBE_TEST_EXE ${WBE_TEST_EXE}
    PARENT_SCOPE
)
