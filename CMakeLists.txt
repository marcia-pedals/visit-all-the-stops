cmake_minimum_required(VERSION 3.16)

project(VisitAllTheStops)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable GTest integration for RapidCheck
set(RC_ENABLE_GTEST ON CACHE BOOL "Enable GTest integration for RapidCheck")

add_subdirectory(third_party/rapidcheck)
include_directories(third_party/rapidcheck/include)
include_directories(third_party/rapidcheck/extras/gtest/include)

add_subdirectory(third_party/csv-parser)
include_directories(third_party/csv-parser/single_include)

add_subdirectory(third_party/abseil-cpp)
include_directories(third_party/abseil-cpp)

add_subdirectory(third_party/googletest)
include_directories(third_party/googletest/googlemock/include)
include_directories(third_party/googletest/googletest/include)

include_directories(third_party/tomlplusplus/include)

# MultiSegment
add_library(MultiSegment src/MultiSegment.cpp)

# World
add_library(World src/World.cpp)
target_link_libraries(World MultiSegment absl::strings absl::str_format absl::time csv)

# Problem
add_library(Problem src/Problem.cpp)
target_link_libraries(Problem World absl::flat_hash_map absl::strings)

# Solver
add_library(Solver src/Solver.cpp)
target_link_libraries(Solver World Problem absl::flat_hash_map absl::strings)

# Config
add_library(Config src/Config.cpp)
target_link_libraries(Config World absl::strings absl::time)

# list_stops
add_executable(list_stops src/list_stops.cpp)
target_link_libraries(list_stops Config World absl::flags absl::flags_parse)

# main
add_executable(main src/main.cpp)
target_link_libraries(main Config MultiSegment World Solver absl::flags absl::flags_parse)


# Enable testing
enable_testing()

# MultiSegment test
add_executable(MultiSegment_test src/MultiSegment_test.cpp)
target_link_libraries(MultiSegment_test gtest_main)
target_link_libraries(MultiSegment_test MultiSegment)
target_link_libraries(MultiSegment_test rapidcheck)
add_test(NAME MultiSegment_test COMMAND MultiSegment_test)
set_tests_properties(MultiSegment_test PROPERTIES ENVIRONMENT "RC_PARAMS=max_success=10000")

# WalkFinder test
add_executable(WalkFinder_test src/WalkFinder_test.cpp)
target_link_libraries(WalkFinder_test gtest_main gmock_main)
target_link_libraries(WalkFinder_test rapidcheck)
add_test(NAME WalkFinder_test COMMAND WalkFinder_test)

# World test
add_executable(World_test src/World_test.cpp)
target_link_libraries(World_test World gtest_main gmock_main)
add_test(NAME World_test COMMAND World_test)
set_tests_properties(World_test PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
