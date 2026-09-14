set(SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/parser.cpp)

set(GEN_TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/gen_test.cpp)

target_sources(${GEN_TEST_TARGET} PRIVATE ${GEN_TEST_SOURCES})
target_sources(${TARGET} PRIVATE ${SOURCES})
