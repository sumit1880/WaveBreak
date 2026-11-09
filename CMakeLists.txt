cmake_minimum_required(VERSION 3.15)
project(SpectrumSentinel VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler Warnings
if(MSVC)
    add_compile_options(/W4 /EHsc)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# Optional AddressSanitizer guard (not on MinGW)
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
if(ENABLE_ASAN)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-fsanitize=address" COMPILER_SUPPORTS_ASAN)
    if(COMPILER_SUPPORTS_ASAN AND NOT MINGW)
        add_compile_options(-fsanitize=address)
        add_link_options(-fsanitize=address)
        message(STATUS "AddressSanitizer enabled")
    else()
        message(STATUS "AddressSanitizer requested but not supported or disabled on MinGW")
    endif()
endif()

# Core static library
set(CORE_SOURCES
    src/config_loader.cpp
    src/fft_processor.cpp
    src/spectrum_analyzer.cpp
    src/signal_generator.cpp
    src/io_writers.cpp
    src/cli.cpp
    src/interference_detector.cpp
    src/event_classifier.cpp
    src/detection_evaluator.cpp
    src/experiment_runner.cpp
)

add_library(spectrum_sentinel_core STATIC ${CORE_SOURCES})
target_include_directories(spectrum_sentinel_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Main CLI executable
add_executable(spectrum_sentinel src/main.cpp)
target_link_libraries(spectrum_sentinel PRIVATE spectrum_sentinel_core)

# Tests & CTest registration
enable_testing()

set(TEST_SOURCES
    tests/main_tests.cpp
    tests/test_fft.cpp
    tests/test_spectrum_analyzer.cpp
    tests/test_signal_generator.cpp
    tests/test_config_loader.cpp
    tests/test_cli_and_io.cpp
    tests/test_detector_and_evaluator.cpp
)

add_executable(spectrum_sentinel_tests ${TEST_SOURCES})
target_include_directories(spectrum_sentinel_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)
target_link_libraries(spectrum_sentinel_tests PRIVATE spectrum_sentinel_core)

add_test(NAME SpectrumSentinelUnitTests COMMAND spectrum_sentinel_tests)
