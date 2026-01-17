cmake_minimum_required(VERSION 3.14)
project(civicc VERSION 1.0 LANGUAGES C CXX)

set(SOURCE_FILES
    src/global/globals.c
    src/print/print.c
    src/scanparse/scanParse.c
    src/optimization/opt_sub.c
    src/context_analysis/function_header.c
    src/context_analysis/declaration_check.c
    src/context_analysis/free_symbols.c
    src/context_analysis/extract_for_iterator.c
    src/context_analysis/definitions.c
    src/to_string.c
    src/context_analysis/init_function.c
    src/context_analysis/heterogeneous_assignments.c
    src/context_analysis/homogeneous_assignments.c
)

set(TEST_FILES
    test/test_interface.c
    test/scanparse_tests.cpp
    test/context_tests.cpp
    test/codegenprep_tests.cpp
    test/behavior_tests.cpp
    test/generation_tests.cpp
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

option(DISABLE_ASAN "Disable address sanitizer" OFF)
option(BUILD_GRAMMAR_GENERATOR
    "Builds a civicc generator from the Grammar Mutator library of Afl.\
Note: Afl is not required to build the generator."
    OFF
)
option(DEBUG_SCANPARSE "Prints debug information from the lexing and parsing to the console." OFF)

option(CIVICC_ASSEMBLER "The path to the civicc assembler 'civas'" "civas")
option(CIVICC_VM "The path to the civicc virtual machine 'civvm'" "civvm")

add_compile_definitions(PROGRAM_CIVAS="${CIVICC_ASSEMBLER}" PROGRAM_CIVVM="${CIVICC_VM}")

set(TEST_DIR "${CMAKE_CURRENT_LIST_DIR}/test")
set(COMPILER "${CMAKE_CURRENT_BINARY_DIR}/civicc")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Check for AFL
include(cmake/afl.cmake)

if(AFL_ENABLED)
    if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
        message(WARNING "Build type 'Debug' is set for Fuzzing. Build type 'Release' is recommend
        as we want to fuzz as fast as possible.")
    endif()

    # We set the scanparse exit code to 1 as it can be used for positive space fuzzing
    add_compile_definitions(YY_EXIT_FAILURE=1)

    # We remove the NDEBUG flags for the AFL builds as assertions are key part of crashes.
    string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    string(REPLACE "-DNDEBUG" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")

    # Add additional optimization options
    add_compile_options(-flto)
    string(TOLOWER ${CMAKE_SYSTEM_PROCESSOR} SYSTEM_PROCESSOR_ARCH)
    if(${SYSTEM_PROCESSOR_ARCH} MATCHES ".*(arm|aarch).*") # ARM architecture
        # See https://developer.arm.com/community/arm-community-blogs/b/tools-software-ides-blog/posts/compiler-flags-across-architectures-march-mtune-and-mcpu
        add_compile_options(-mcpu=native)
    elseif(${SYSTEM_PROCESSOR_ARCH} MATCHES ".*(amd64|86).*") # x86 architecture
        add_compile_options(-march=native -mtune=native)
    else()
        message(WARNING "Native Architecture optimizations not supported on: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()
endif()


include(FetchContent)
set(INSTALL_GTEST OFF)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest
    GIT_TAG        v1.17.0
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(googletest)

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
if(NOT AFL_ENABLED)
    if(DEBUG_SCANPARSE)
        add_compile_definitions($<$<CONFIG:Debug>:DEBUG_SCANPARSE=1>)
    endif()
endif()
add_compile_definitions(PROJECT_DIRECTORY="${PROJECT_SOURCE_DIR}")
message(STATUS "Set Definition 'PROJECT_DIRECTORY=${PROJECT_SOURCE_DIR}'")

add_compile_options(-Wall -Wextra -pedantic -Wno-unused-function -Wconversion)
add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wno-strict-prototypes>)

# Enable address sanitizer
if(NOT DISABLE_ASAN AND NOT AFL_ENABLED)
    if(APPLE)
       message(STATUS "Sanitizer: Can not build with Sanitizer on Apple architecture.")
    else()
        add_compile_options(
            $<$<CONFIG:Debug>:-fsanitize=address,undefined>
        )
        add_link_options(
            $<$<CONFIG:Debug>:-fsanitize=address,undefined>
        )
    endif()
endif()

add_library("civicc_lib${AFL_EXT}" STATIC ${FLEX_CivicLexer_OUTPUTS} ${BISON_CivicParser_OUTPUTS} ${SOURCE_FILES})
set_target_properties("civicc_lib${AFL_EXT}" PROPERTIES OUTPUT_NAME "civicc${AFL_EXT}")
coconut_target_generate("civicc_lib${AFL_EXT}" "${CMAKE_CURRENT_LIST_DIR}/src/main.ccn" dynamic)
target_include_directories("civicc_lib${AFL_EXT}"
    PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src"
)

add_executable("civicc${AFL_EXT}" src/main.c)
target_link_libraries("civicc${AFL_EXT}" PRIVATE "civicc_lib${AFL_EXT}")
coconut_add_includes("civicc${AFL_EXT}")
target_include_directories("civicc${AFL_EXT}"
    PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src"
)

add_executable("civicc_scanparse${AFL_EXT}" ${FUZZ_FILES} test/fuzz_slice.cpp)
target_compile_definitions("civicc_scanparse${AFL_EXT}"
    PRIVATE SLICE_TARGET=1)
target_link_libraries("civicc_scanparse${AFL_EXT}" PRIVATE "civicc_lib${AFL_EXT}")
coconut_add_includes("civicc_scanparse${AFL_EXT}")
target_include_directories("civicc_scanparse${AFL_EXT}"
    PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src" "${CMAKE_CURRENT_LIST_DIR}/test"
)

add_executable("civicc_context${AFL_EXT}" ${FUZZ_FILES} test/fuzz_slice.cpp)
target_compile_definitions("civicc_context${AFL_EXT}"
    PRIVATE SLICE_TARGET=2)
target_link_libraries("civicc_context${AFL_EXT}" PRIVATE "civicc_lib${AFL_EXT}")
coconut_add_includes("civicc_context${AFL_EXT}")
target_include_directories("civicc_context${AFL_EXT}"
    PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src" "${CMAKE_CURRENT_LIST_DIR}/test"
)

add_executable("civicc_codegenprep${AFL_EXT}" ${FUZZ_FILES} test/fuzz_slice.cpp)
target_compile_definitions("civicc_codegenprep${AFL_EXT}"
    PRIVATE SLICE_TARGET=3)
target_link_libraries("civicc_codegenprep${AFL_EXT}" PRIVATE "civicc_lib${AFL_EXT}")
coconut_add_includes("civicc_codegenprep${AFL_EXT}")
target_include_directories("civicc_codegenprep${AFL_EXT}"
    PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src" "${CMAKE_CURRENT_LIST_DIR}/test"
)

if(NOT AFL_ENABLED)
    add_executable(tests ${TEST_FILES}) # No need to apply afl to tests
    target_link_libraries(tests PRIVATE gmock_main civicc_lib)
    coconut_add_includes(tests)
    target_include_directories(tests
        PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src" "${CMAKE_CURRENT_LIST_DIR}/test"
    )

    # === Google Test integration ===
    enable_testing()
    include(GoogleTest)
    gtest_discover_tests(tests)

endif()

add_custom_target(dot
    dot -Tpng ccngen/ast.dot > ast.png
    COMMENT "Generate a png of your ast based on the generated dot diagram."
)

if(BUILD_GRAMMAR_GENERATOR)
    add_custom_target(grammar_generator
        COMMENT "Dummy target for the grammar generation, the grammar is build when the option 'BUILD_GRAMMAR_GENERATOR' is set."
    )
    setup_grammar("grammars/civicc.json")
endif()
