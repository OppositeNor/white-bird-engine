add_subdirectory(core)
add_subdirectory(platform)
add_subdirectory(utils_test)

set(WBE_BUILD_CONFIG_SCRIPTS ${WBE_BUILD_CONFIG_SCRIPTS}
    PARENT_SCOPE
)
