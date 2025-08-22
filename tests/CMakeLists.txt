add_subdirectory(core)
add_subdirectory(platform)
add_subdirectory(utils_test)
add_subdirectory(resource)

set(WBE_BUILD_CONFIG_SCRIPTS ${WBE_BUILD_CONFIG_SCRIPTS}
    PARENT_SCOPE
)

set(WBE_TEST_EXE ${WBE_TEST_EXE}
    PARENT_SCOPE
)
