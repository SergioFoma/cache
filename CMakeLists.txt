cmake_minimum_required(VERSION 3.11)

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
  URL https://github.com/nlohmann/json/archive/refs/tags/v3.12.0.zip
)

FetchContent_Declare(
  cli11
  URL https://github.com/CLIUtils/CLI11/archive/refs/tags/v2.7.2.zip
)

FetchContent_MakeAvailable(googletest json cli11)


set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL
    "Generate compile_commands.json" FORCE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-O2 -Wall")

# ===== cache =====
set(TARGET cache)
add_executable(${TARGET})
target_include_directories(${TARGET} PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(${TARGET} PRIVATE nlohmann_json::nlohmann_json CLI11::CLI11)

# ===== end-to-end gen =====
set(GEN_TEST_TARGET gen_test)
add_executable(${GEN_TEST_TARGET})

# ===== tests =====
enable_testing()

include(GoogleTest)

set(TEST_TARGET cache_algorithm_test)
add_executable(${TEST_TARGET})

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src)

target_include_directories(${TEST_TARGET} PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(cache_algorithm_test PRIVATE GTest::gtest_main)

gtest_discover_tests(cache_algorithm_test)
