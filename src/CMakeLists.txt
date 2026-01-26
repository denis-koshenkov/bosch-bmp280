add_library(driver INTERFACE)

target_sources(driver INTERFACE
    bmp280.c
)

target_include_directories(driver INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
