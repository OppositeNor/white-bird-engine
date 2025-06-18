file(GLOB utils_test_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(utils_test
    ${utils_test_src}
)

target_link_libraries(utils_test
    ${libs}
    gtest_main
)

set(WBE_BUILD_CONFIG_SCRIPTS ${WBE_BUILD_CONFIG_SCRIPTS}
    PARENT_SCOPE
)
