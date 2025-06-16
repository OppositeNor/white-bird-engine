file(GLOB wbe_parser_test_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(wbe_parser_test
    ${wbe_parser_test_src}
)

target_link_libraries(wbe_parser_test
    ${libs}
    gtest_main
)


