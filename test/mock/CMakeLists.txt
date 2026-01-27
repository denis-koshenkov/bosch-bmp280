target_sources(run_tests PRIVATE
    mock_cfg_functions.cpp
    mock_complete_cb.cpp
)

target_include_directories(run_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
