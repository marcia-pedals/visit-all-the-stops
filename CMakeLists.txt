cmake_minimum_required(VERSION 3.10)

project(VisitAllTheStops)

set(CMAKE_CXX_STANDARD 20)

message(STATUS "BOOST_ROOT: $ENV{BOOST_ROOT}")
set(BOOST_ROOT $ENV{BOOST_ROOT})

find_package(Boost 1.79.0 REQUIRED COMPONENTS system filesystem)
include_directories(${Boost_INCLUDE_DIRS})

# Enable GTest integration for RapidCheck
set(RC_ENABLE_GTEST ON CACHE BOOL "Enable GTest integration for RapidCheck")

add_subdirectory(third_party/rapidcheck)

include_directories(third_party/rapidcheck/include)
include_directories(third_party/rapidcheck/extras/gtest/include)

# MultiSegment
add_library(MultiSegment src/MultiSegment.cpp)
target_link_libraries(MultiSegment ${Boost_LIBRARIES})

# Add executable for the main program
add_executable(hello src/main.cpp)
target_link_libraries(hello MultiSegment)


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

# Example test
add_executable(example_test src/main_test.cpp)
target_link_libraries(example_test gtest_main)
add_test(NAME example_test COMMAND example_test)

# MultiSegment test
add_executable(MultiSegment_test src/MultiSegment_test.cpp)
target_link_libraries(MultiSegment_test gtest_main)
target_link_libraries(MultiSegment_test MultiSegment)
target_link_libraries(MultiSegment_test rapidcheck)
add_test(NAME MultiSegment_test COMMAND MultiSegment_test)
set_tests_properties(MultiSegment_test PROPERTIES ENVIRONMENT "RC_PARAMS=max_success=10000")
