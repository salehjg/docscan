cmake_minimum_required(VERSION 3.25)
project(docscan
    VERSION 0.1.0
    DESCRIPTION "Turn photos of documents into clean, straight PDF pages"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(DOCSCAN_BUILD_TESTS "Build the unit tests (needs Catch2)" OFF)
set(DOCSCAN_UVDOC_MODEL "" CACHE FILEPATH
    "UVDoc weights (model/best_model.pkl from github.com/tanguymagne/UVDoc) to install for the crop filter")

find_package(OpenCV 5 REQUIRED COMPONENTS core imgproc imgcodecs photo geometry)
find_package(CLI11 CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenMP REQUIRED COMPONENTS CXX)
find_package(CURL 7.85 REQUIRED)
find_package(OpenSSL REQUIRED COMPONENTS Crypto)

include(GNUInstallDirs)

configure_file(src/Version.hpp.in generated/Version.hpp @ONLY)

add_library(docscan_core STATIC
    src/app/Application.cpp
    src/app/CommandLine.cpp
    src/core/Download.cpp
    src/core/FilterSpec.cpp
    src/core/Log.cpp
    src/core/Settings.cpp
    src/core/Text.cpp
    src/export/ImageExporter.cpp
    src/export/PageEncoder.cpp
    src/export/PaperSize.cpp
    src/export/PdfExporter.cpp
    src/export/Process.cpp
    src/export/TempDir.cpp
    src/filters/BwFilter.cpp
    src/filters/ContrastFilter.cpp
    src/filters/CropFilter.cpp
    src/filters/DenoiseFilter.cpp
    src/filters/FilterRegistry.cpp
    src/filters/GrayFilter.cpp
    src/filters/LightFilter.cpp
    src/filters/ResizeFilter.cpp
    src/filters/RotateFilter.cpp
    src/filters/SharpenFilter.cpp
    src/ml/Conv2d.cpp
    src/ml/TorchCheckpoint.cpp
    src/ml/UVDocNet.cpp
    src/pipeline/DebugWriter.cpp
    src/pipeline/Inputs.cpp
    src/pipeline/Pipeline.cpp
    src/pipeline/PipelineBuilder.cpp
    src/pipeline/Profile.cpp
    src/pipeline/Scanner.cpp
    src/vision/Binarizer.cpp
    src/vision/ContourPageFinder.cpp
    src/vision/EdgeSnapper.cpp
    src/vision/Imaging.cpp
    src/vision/MeshSnapper.cpp
    src/vision/PageMesh.cpp
    src/vision/Quad.cpp
    src/vision/UVDocPageFinder.cpp
)
target_include_directories(docscan_core PUBLIC src ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_include_directories(docscan_core SYSTEM PUBLIC ${OpenCV_INCLUDE_DIRS})
target_link_libraries(docscan_core PUBLIC ${OpenCV_LIBS} Threads::Threads PRIVATE CLI11::CLI11 OpenMP::OpenMP_CXX
    CURL::libcurl OpenSSL::Crypto)
target_compile_options(docscan_core PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(docscan_core PRIVATE DOCSCAN_DATA_DIR="${CMAKE_INSTALL_FULL_DATADIR}/docscan")

add_executable(docscan src/main.cpp)
target_link_libraries(docscan PRIVATE docscan_core)
target_compile_options(docscan PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS docscan RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(DOCSCAN_UVDOC_MODEL)
    install(FILES "${DOCSCAN_UVDOC_MODEL}" DESTINATION ${CMAKE_INSTALL_DATADIR}/docscan RENAME uvdoc.pkl)
    install(FILES third_party/UVDoc/LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/docscan RENAME uvdoc-LICENSE)
endif()

if(DOCSCAN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
