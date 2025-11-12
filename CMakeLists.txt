cmake_minimum_required(VERSION 3.13)
project(civicc VERSION 1.0 LANGUAGES C)

set(SOURCE_FILES
    src/main.c
    src/global/globals.c
    src/print/print.c
    src/scanparse/scanParse.c
    src/demo/opt_sub.c
    src/demo/rename_identifiers.c
    src/demo/sum_ints.c
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

set(TEST_DIR "${CMAKE_CURRENT_LIST_DIR}/test")
set(COMPILER "${CMAKE_CURRENT_BINARY_DIR}/civicc")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DISABLE_ASAN "Disable address sanitizer")

# These will only work after you received the testing framework from us.
enable_testing()
add_test(NAME "basic" COMMAND "${TEST_COMMAND}/run.bash" "${COMPILER}" basic WORKING_DIRECTORY "${TEST_DIR}")
add_test(NAME "nested_funs" COMMAND "${TEST_COMMAND}/run.bash" "${COMPILER}" nested_funs WORKING_DIRECTORY "${TEST_DIR}")
add_test(NAME "arrays" COMMAND "${TEST_COMMAND}/run.bash" "${COMPILER}" arrays WORKING_DIRECTORY "${TEST_DIR}")


find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

bison_target(CivicParser src/scanparse/parser.y "${CMAKE_CURRENT_BINARY_DIR}/parser.c"
    DEFINES_FILE "${CMAKE_CURRENT_BINARY_DIR}/parser.h" VERBOSE)

flex_target(CivicLexer src/scanparse/lexer.l "${CMAKE_CURRENT_BINARY_DIR}/lexer.c"
    DEFINES_FILE "${CMAKE_CURRENT_BINARY_DIR}/lexer.h")
ADD_FLEX_BISON_DEPENDENCY(CivicLexer CivicParser)

include(cmake/coconut.cmake)

# Whenever you add a file, add it here too.
add_executable(civicc ${FLEX_CivicLexer_OUTPUTS} ${BISON_CivicParser_OUTPUTS} ${SOURCE_FILES})

target_compile_options(civicc PRIVATE
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Werror -Wall -Wextra -pedantic -Wno-unused-function>
)

# Enable address sanitizer
if(NOT DISABLE_ASAN)
    target_compile_options(civicc PRIVATE
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:$<$<CONFIG:Debug>:-fsanitize=address>>
    )

    target_link_options(civicc PRIVATE
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:$<$<CONFIG:Debug>:-fsanitize=address>>
    )
endif()

coconut_target_generate(civicc "${CMAKE_CURRENT_LIST_DIR}/src/main.ccn" dynamic)
target_include_directories(civicc
    PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src"
)

add_custom_target(dot
    dot -Tpng ccngen/ast.dot > ast.png
    COMMENT "Generate a png of your ast based on the generated dot diagram."
)
