file(GLOB wbe_logging_test_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(wbe_logging_test
    ${wbe_logging_test_src}
)

target_link_libraries(wbe_logging_test
    ${libs}
    gtest_main
)

set(WBE_BUILD_CONFIG_SCRIPTS ${WBE_BUILD_CONFIG_SCRIPTS}
    ${CMAKE_CURRENT_SOURCE_DIR}/config.py
    PARENT_SCOPE
)
