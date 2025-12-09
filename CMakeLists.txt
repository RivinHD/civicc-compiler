cmake_minimum_required(VERSION 3.14)
project(civicc VERSION 1.0 LANGUAGES C CXX)

set(SOURCE_FILES
    src/global/globals.c
    src/print/print.c
    src/scanparse/scanParse.c
    src/optimization/opt_sub.c
)

set(TEST_FILES
    test/test_interface.c
    test/scanparse_tests.cpp
)

set(FUZZ_FILES
    test/test_interface.c
)

get_property(IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(IS_MULTI_CONFIG)
    message(NOTICE "Using multi-config generator. Compile with: cmake --build build --config [Debug|Release] --target <target>")
else()
    message(NOTICE "Using single-config generator. Generate with: cmake -B build -DCMAKE_BUILD_TYPE=[Debug|Release]")
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Debug")
        message(WARNING "No Build type is set. Using ${CMAKE_BUILD_TYPE}!")
    endif()
endif()

message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")

option(DISABLE_ASAN "Disable address sanitizer")

set(TEST_DIR "${CMAKE_CURRENT_LIST_DIR}/test")
set(COMPILER "${CMAKE_CURRENT_BINARY_DIR}/civicc")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Check for AFL
include(cmake/afl.cmake)

if(AFL_ENABLED)
    # We set the scanparse exit code to 1 as it can be used for positive space fuzzing
    add_compile_definitions(YY_EXIT_FAILURE=1)
endif()

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest
    GIT_TAG        v1.17.0
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(googletest)


# These will only work after you received the testing framework from us.
enable_testing()
# add_test(NAME "basic" COMMAND "${TEST_COMMAND}/run.bash" "${COMPILER}" basic WORKING_DIRECTORY "${TEST_DIR}")
# add_test(NAME "nested_funs" COMMAND "${TEST_COMMAND}/run.bash" "${COMPILER}" nested_funs WORKING_DIRECTORY "${TEST_DIR}")
# add_test(NAME "arrays" COMMAND "${TEST_COMMAND}/run.bash" "${COMPILER}" arrays WORKING_DIRECTORY "${TEST_DIR}")


find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

bison_target(CivicParser src/scanparse/parser.y "${CMAKE_CURRENT_BINARY_DIR}/parser.c"
    DEFINES_FILE "${CMAKE_CURRENT_BINARY_DIR}/parser.h"
    VERBOSE
    COMPILE_FLAGS "-Wcounterexamples")

flex_target(CivicLexer src/scanparse/lexer.l "${CMAKE_CURRENT_BINARY_DIR}/lexer.c"
    DEFINES_FILE "${CMAKE_CURRENT_BINARY_DIR}/lexer.h")
ADD_FLEX_BISON_DEPENDENCY(CivicLexer CivicParser)

include(cmake/coconut.cmake)

# Add compile options before all targets but after the includes to only apply them to the targets
add_compile_definitions(PROJECT_DIRECTORY="${PROJECT_SOURCE_DIR}")
message(STATUS "Set Definition 'PROJECT_DIRECTORY=${PROJECT_SOURCE_DIR}'")

add_compile_options(-Wall -Wextra -pedantic -Wno-unused-function)
add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wno-strict-prototypes>)

# Enable address sanitizer
if(NOT DISABLE_ASAN AND NOT AFL_ENABLED)
    add_compile_options(
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )
    add_link_options(
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )
endif()

# Whenever you add a file, add it here too.
add_executable("civicc${AFL_EXT}"
    ${FLEX_CivicLexer_OUTPUTS} ${BISON_CivicParser_OUTPUTS} ${SOURCE_FILES}
    src/main.c
)
coconut_target_generate("civicc${AFL_EXT}" "${CMAKE_CURRENT_LIST_DIR}/src/main.ccn" dynamic)
target_include_directories("civicc${AFL_EXT}"
    PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src"
)

add_library("civicc_lib${AFL_EXT}" STATIC ${FLEX_CivicLexer_OUTPUTS} ${BISON_CivicParser_OUTPUTS} ${SOURCE_FILES})
coconut_target_generate("civicc_lib${AFL_EXT}" "${CMAKE_CURRENT_LIST_DIR}/src/main.ccn" dynamic)
target_include_directories("civicc_lib${AFL_EXT}"
    PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src"
)

add_executable("civicc_scanparse${AFL_EXT}" ${FUZZ_FILES} test/fuzz_scanparse_slice.cpp)
target_link_libraries("civicc_scanparse${AFL_EXT}" PRIVATE "civicc_lib${AFL_EXT}")
coconut_add_includes("civicc_scanparse${AFL_EXT}")
target_include_directories("civicc_scanparse${AFL_EXT}"
    PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src" "${CMAKE_CURRENT_LIST_DIR}/test"
)

if(NOT AFL_ENABLED)
    add_executable(tests ${TEST_FILES}) # No need to apply afl to tests
    target_link_libraries(tests PRIVATE gtest_main civicc_lib)
    coconut_add_includes(tests)
    target_include_directories(tests
        PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src" "${CMAKE_CURRENT_LIST_DIR}/test"
    )

    # === Google Test integration ===
    include(GoogleTest)
    gtest_discover_tests(tests)

endif()

add_custom_target(dot
    dot -Tpng ccngen/ast.dot > ast.png
    COMMENT "Generate a png of your ast based on the generated dot diagram."
)

