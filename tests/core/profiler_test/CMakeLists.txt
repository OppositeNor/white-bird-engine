file(GLOB wbe_profiler_test_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(wbe_profiler_test
    ${wbe_profiler_test_src}
)

target_link_libraries(wbe_profiler_test
    ${libs}
    gtest_main
)

set(WBE_BUILD_CONFIG_SCRIPTS ${WBE_BUILD_CONFIG_SCRIPTS}
    ${CMAKE_CURRENT_SOURCE_DIR}/config.py
    PARENT_SCOPE
)


