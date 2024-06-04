cmake_minimum_required(VERSION 3.10)

project(HelloWorld)

set(CMAKE_CXX_STANDARD 17)

# Add executable for the main program
add_executable(hello src/main.cpp)

# Enable testing
enable_testing()

# Add Google Test
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/release-1.11.0.tar.gz
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Add test executable
add_executable(tests src/main_test.cpp)

# Link test executable against gtest & gtest_main
target_link_libraries(tests gtest_main)

# Add test
add_test(NAME example_test COMMAND tests)
