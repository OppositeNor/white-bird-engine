file(GLOB allocator_test_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(allocator_test
    ${allocator_test_src}
)

target_link_libraries(allocator_test
    ${libs}
    gtest_main
)
