file(GLOB wbe_platform_linux_test_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(wbe_platform_linux_test
    ${wbe_platform_linux_test_src}
)

target_link_libraries(wbe_platform_linux_test
    ${libs}
    gtest_main
)


