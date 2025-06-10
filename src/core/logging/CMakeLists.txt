file(GLOB logging_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(LOGGING_LIB STATIC
    ${logging_src}
)

set(libs ${libs}
    LOGGING_LIB
    PARENT_SCOPE
)


