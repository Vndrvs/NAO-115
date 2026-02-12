cmake_minimum_required(VERSION 3.14)
project(NAO-115)

# Set C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------
# 1. Fetch GoogleTest automatically
# ---------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ---------------------------------------------------------
# 2. Define the Main Library (Your Core Logic)
# ---------------------------------------------------------
# We create a library so both the Main App and the Tests can use the code
# without compiling it twice.

# Find all source files in src, excluding main.cpp
file(GLOB_RECURSE SOURCE_FILES "NAO-115/src/*.cpp")
list(FILTER SOURCE_FILES EXCLUDE REGEX "NAO-115/src/main.cpp")
list(FILTER SOURCE_FILES EXCLUDE REGEX "NAO-115/src/benchmark/.*")

# Create the library
add_library(nao_core ${SOURCE_FILES})
target_include_directories(nao_core PUBLIC "NAO-115/src")

# ---------------------------------------------------------
# 3. The Main Executable (The Game/Solver)
# ---------------------------------------------------------
add_executable(nao_run "NAO-115/src/main.cpp")
target_link_libraries(nao_run PRIVATE nao_core)

# ---------------------------------------------------------
# 4. The Test Executable (Google Test)
# ---------------------------------------------------------
enable_testing()

include(GoogleTest)

add_executable(nao_tests "NAO-115_Tests/main.cpp")
target_link_libraries(nao_tests PRIVATE nao_core GTest::gtest_main)
gtest_discover_tests(nao_tests)

add_executable(bucketer_tests "NAO-115_Tests/bucketer/test_bucketer.cpp")
target_link_libraries(bucketer_tests PRIVATE nao_core GTest::gtest_main)
target_include_directories(bucketer_tests PRIVATE "${CMAKE_SOURCE_DIR}/NAO-115/src")
gtest_discover_tests(bucketer_tests)

add_executable(evaluator_tests "NAO-115_Tests/evaluator/test_evaluator.cpp")
target_link_libraries(evaluator_tests PRIVATE nao_core GTest::gtest_main)
gtest_discover_tests(evaluator_tests)