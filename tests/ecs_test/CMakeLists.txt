file(GLOB ecs_test_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(ecs_test
    ${ecs_test_src}
)

target_link_libraries(ecs_test
    ${libs}
    gtest_main
)
