add_executable(run_tests)

target_sources(run_tests PRIVATE
    main.cpp
    bmp280.cpp
)

set(TESTS OFF) # Disable cpputest self-tests
add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../deps/cpputest
    ${CMAKE_CURRENT_BINARY_DIR}/cpputest
)

target_link_libraries(run_tests PRIVATE
    CppUTest
    CppUTestExt
    driver
)
