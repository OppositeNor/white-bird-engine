file(GLOB wbe_logging_src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

set(wbe_core_src
    ${wbe_core_src}
    ${wbe_logging_src}
    PARENT_SCOPE
)

set(WBE_BUILD_CONFIG_SCRIPTS ${WBE_BUILD_CONFIG_SCRIPTS}
    ${CMAKE_CURRENT_SOURCE_DIR}/config.py
    PARENT_SCOPE
)

