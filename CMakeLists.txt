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

find_package(OpenCV 5 REQUIRED COMPONENTS core imgproc imgcodecs photo geometry)
find_package(CLI11 CONFIG REQUIRED)
find_package(Threads REQUIRED)

configure_file(src/Version.hpp.in generated/Version.hpp @ONLY)

add_library(docscan_core STATIC
    src/app/Application.cpp
    src/app/CommandLine.cpp
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
    src/vision/Quad.cpp
)
target_include_directories(docscan_core PUBLIC src ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_include_directories(docscan_core SYSTEM PUBLIC ${OpenCV_INCLUDE_DIRS})
target_link_libraries(docscan_core PUBLIC ${OpenCV_LIBS} Threads::Threads PRIVATE CLI11::CLI11)
target_compile_options(docscan_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(docscan src/main.cpp)
target_link_libraries(docscan PRIVATE docscan_core)
target_compile_options(docscan PRIVATE -Wall -Wextra -Wpedantic)

include(GNUInstallDirs)
install(TARGETS docscan RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(DOCSCAN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
