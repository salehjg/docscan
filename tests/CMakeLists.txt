find_package(Catch2 3 REQUIRED)

add_executable(docscan_tests
    CropFilterTest.cpp
    FilterSpecTest.cpp
    PaperSizeTest.cpp
    PipelineBuilderTest.cpp
    QuadTest.cpp
    SettingsTest.cpp
    TextTest.cpp
)
target_link_libraries(docscan_tests PRIVATE docscan_core Catch2::Catch2WithMain)
target_compile_options(docscan_tests PRIVATE -Wall -Wextra -Wpedantic)

include(Catch)
catch_discover_tests(docscan_tests)
