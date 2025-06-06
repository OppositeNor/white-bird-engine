file(GLOB reference_test_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(reference_test
    ${reference_test_src}
)

target_link_libraries(reference_test
    ${libs}
    gtest_main
)

