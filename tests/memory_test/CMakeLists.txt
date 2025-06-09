file(GLOB memory_test_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(memory_test
    ${memory_test_src}
)

target_link_libraries(memory_test
    ${libs}
    gtest_main
)

