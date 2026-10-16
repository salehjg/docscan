find_package(Catch2 3 REQUIRED)

add_executable(docscan_tests
    Conv2dTest.cpp
    CropFilterTest.cpp
    DownloadTest.cpp
    FilterSpecTest.cpp
    MeshSnapperTest.cpp
    PageMeshTest.cpp
    PaperSizeTest.cpp
    PipelineBuilderTest.cpp
    QuadTest.cpp
    SettingsTest.cpp
    TextTest.cpp
    TorchCheckpointTest.cpp
)
target_link_libraries(docscan_tests PRIVATE docscan_core Catch2::Catch2WithMain)
target_compile_options(docscan_tests PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(docscan_tests PRIVATE DOCSCAN_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data")

include(Catch)
catch_discover_tests(docscan_tests)
