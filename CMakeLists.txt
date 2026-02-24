cmake_minimum_required(VERSION 3.14)
project(NAO-115)

# Set C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch Googletest
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)


# Find all source files
file(GLOB_RECURSE SOURCE_FILES "NAO-115/src/*.cpp" "NAO-115/src/*.hpp" "NAO-115/src/*.h")

list(FILTER SOURCE_FILES EXCLUDE REGEX "NAO-115/src/main.cpp")
list(FILTER SOURCE_FILES EXCLUDE REGEX "NAO-115/src/benchmark/.*")

# PRESERVE FOLDER STRUCTURE IN XCODE
foreach(file_path ${SOURCE_FILES})
    file(RELATIVE_PATH relative_path "${CMAKE_CURRENT_SOURCE_DIR}/NAO-115/src" "${file_path}")
    get_filename_component(dir_path "${relative_path}" DIRECTORY)
    string(REPLACE "/" "\\" group_path "${dir_path}")
    source_group("${group_path}" FILES "${file_path}")
endforeach()

# Create the library
add_library(nao_core ${SOURCE_FILES})
target_include_directories(nao_core PUBLIC "NAO-115/src")

# OpenMP support for AppleClang (macOS)
if(APPLE)
    find_library(LIBOMP_LIB omp HINTS /opt/homebrew/opt/libomp/lib)

    target_compile_options(nao_core
        PUBLIC
            -Xpreprocessor -fopenmp
            -I/opt/homebrew/opt/libomp/include
    )

    target_link_libraries(nao_core
        PUBLIC
            ${LIBOMP_LIB}
    )
else()
    find_package(OpenMP REQUIRED)

    target_link_libraries(nao_core
        PUBLIC
            OpenMP::OpenMP_CXX
    )
endif()


# The Game/Solver
add_executable(nao_run "NAO-115/src/main.cpp")
target_link_libraries(nao_run PRIVATE nao_core)

# Tests
enable_testing()
include(GoogleTest)

# Main Tests
add_executable(nao_tests "NAO-115_Tests/main.cpp")
target_link_libraries(nao_tests PRIVATE nao_core GTest::gtest_main)
add_test(NAME NaoTests COMMAND nao_tests)

# Bucketer Tests
add_executable(bucketer_tests "NAO-115_Tests/bucketer/test_bucketer.cpp")
target_link_libraries(bucketer_tests PRIVATE nao_core GTest::gtest_main)
add_test(NAME BucketerTests COMMAND bucketer_tests)

# Evaluator Tests
add_executable(evaluator_tests "NAO-115_Tests/evaluator/test_evaluator.cpp")
target_link_libraries(evaluator_tests PRIVATE nao_core GTest::gtest_main)
add_test(NAME EvaluatorTests COMMAND evaluator_tests)