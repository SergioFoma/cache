cmake_minimum_required(VERSION 4.4)

project(cache)

set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# deps
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/v1.18.x.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/archive/refs/heads/develop.zip
)

FetchContent_MakeAvailable(googletest json)


set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL
    "Generate compile_commands.json" FORCE)

set(TARGET cache)

add_executable(${TARGET})
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-O2 -Wall -Wextra")

target_include_directories(${TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${})
target_link_libraries(${TARGET} PRIVATE nlohmann_json::nlohmann_json)

include_directories("include")

#tests 
enable_testing()

include(GoogleTest)

add_executable(cache_algorithm_test ${CMAKE_CURRENT_SOURCE_DIR}/src/tests/cache_algorithm.cpp)
target_link_libraries(cache_algorithm_test PRIVATE GTest::gtest_main)

gtest_discover_tests(cache_algorithm_test)
