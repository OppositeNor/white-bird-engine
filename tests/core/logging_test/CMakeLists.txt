file(GLOB logging_test_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(logging_test
    ${logging_test_src}
)

target_link_libraries(logging_test
    ${libs}
    gtest_main
)
