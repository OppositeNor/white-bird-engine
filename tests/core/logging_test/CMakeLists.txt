set(TEST_NAME wbe_logging_test)

file(GLOB ${TEST_NAME}_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(${TEST_NAME}
    ${${TEST_NAME}_src}
)

target_link_libraries(${TEST_NAME}
    ${libs}
    gtest_main
)

set(WBE_BUILD_CONFIG_SCRIPTS ${WBE_BUILD_CONFIG_SCRIPTS}
    ${CMAKE_CURRENT_SOURCE_DIR}/config.py
    PARENT_SCOPE
)

set(WBE_TEST_EXE ${WBE_TEST_EXE}
    ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}
    PARENT_SCOPE
)
