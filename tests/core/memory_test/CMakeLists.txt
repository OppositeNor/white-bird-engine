file(GLOB wbe_memory_test_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(wbe_memory_test
    ${wbe_memory_test_src}
)

target_link_libraries(wbe_memory_test
    ${libs}
    gtest_main
)

