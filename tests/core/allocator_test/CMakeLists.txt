file(GLOB wbe_allocator_test_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(wbe_allocator_test
    ${wbe_allocator_test_src}
)

target_link_libraries(wbe_allocator_test
    ${libs}
    gtest_main
)
