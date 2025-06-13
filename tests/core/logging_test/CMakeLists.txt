file(GLOB wbe_logging_test_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(wbe_logging_test
    ${wbe_logging_test_src}
)

target_link_libraries(wbe_logging_test
    ${libs}
    gtest_main
)
