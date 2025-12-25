find_program(AFL_C_COMPILER NAMES afl-cc)
find_program(AFL_CXX_COMPILER NAMES afl-c++)

if(((CMAKE_C_COMPILER STREQUAL AFL_C_COMPILER) AND (CMAKE_CXX_COMPILER STREQUAL AFL_CXX_COMPILER)))
    message(STATUS "Enabled AFL fuzzing in CLang-LTO mode")
    add_compile_options(--afl-lto)
    add_link_options(--afl-lto)
    set(AFL_ENABLED ON)
else()
    set(AFL_ENABLED OFF)
endif()

function(setup_grammar GRAMMAR_FILE)
    set(ANTLR_JAR_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/antlr-4.8-complete.jar")
    set(GRAMMAR_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${GRAMMAR_FILE}")

    message(STATUS "Downloading antlr-4.8-complete.jar to: '${ANTLR_JAR_LOCATION}'")
    file(DOWNLOAD https://www.antlr.org/download/antlr-4.8-complete.jar "${ANTLR_JAR_LOCATION}"
        EXPECTED_HASH SHA256=73a49d6810d903aa4827ee32126937b85d3bebec0a8e679b0dd963cbcc49ba5a
        STATUS DOWNLOAD_RESULT
    )
    message(STATUS "Download Status antlr-4.8-complete.jar: '${DOWNLOAD_RESULT}'")

    include(FetchContent)
    FetchContent_Declare(
        grammar_mutator
        GIT_REPOSITORY https://github.com/AFLplusplus/Grammar-Mutator.git
        GIT_TAG        05d8f537f8d656f0754e7ad5dcc653c42cb4f8ff
        EXCLUDE_FROM_ALL
        SOURCE_SUBDIR "This_path_does_no_exist"
    )
    FetchContent_MakeAvailable(grammar_mutator)

    FetchContent_GetProperties(grammar_mutator
        SOURCE_DIR GRAMMAR_MUTATOR_SOURCE_DIR
        BINARY_DIR GRAMMAR_MUTATOR_BINARY_DIR
    )

    # Renew as the fetch content overrides this variable
    set(ANTLR_JAR_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/antlr-4.8-complete.jar")

    get_filename_component(GRAMMAR_FILENAME ${GRAMMAR_FILE} NAME_WE)
    string(TOLOWER ${GRAMMAR_FILENAME} GRAMMAR_FILENAME)
    STRING(REGEX REPLACE "[_.-].*" "" GRAMMAR_FILENAME ${GRAMMAR_FILENAME})
    message(STATUS "Selected grammar name: ${GRAMMAR_FILENAME} (from ${GRAMMAR_FILE})")

    include(ProcessorCount)
    ProcessorCount(CPU_COUNT)

    # Do NOT built with afl
    message(STATUS "Build the Grammar-Mutator project")
    message(STATUS "Parallel Build: ${CPU_COUNT} cores")
    execute_process(COMMAND ${CMAKE_COMMAND} -E env CC=clang LD=clang++ AR=ar RANLIB=ranlib AS=as CXX=clang++ CCFLAGS="-std=c++17 -fPIC -Wno-tautological-compare -Wno-overloaded-virtual" CXXFLAGS="-std=c++17 -fPIC -Wno-tautological-compare -Wno-overloaded-virtual" LDFLAGS="-shared" -- make ANTLR_JAR_LOCATION=${ANTLR_JAR_LOCATION} GRAMMAR_FILE=${GRAMMAR_FILE} -j ${CPU_COUNT}
        WORKING_DIRECTORY "${GRAMMAR_MUTATOR_SOURCE_DIR}"
        OUTPUT_QUIET
    )

    # GRAMMAR_FILENAME comes from grammar_mutator cmake
    set(GRAMMAR_LIB_FILENAME "libgrammarmutator-${GRAMMAR_FILENAME}.so")
    message(STATUS "Search grammer lib: '${GRAMMAR_LIB_FILENAME}'")
    find_file(GRAMMAR_LIB "${GRAMMAR_LIB_FILENAME}"
        PATHS "${GRAMMAR_MUTATOR_SOURCE_DIR}"
    )
    message(STATUS "Found grammer lib at: '${GRAMMAR_LIB}'")

    set(GRAMMAR_GEN_FILENAME "grammar_generator-${GRAMMAR_FILENAME}")
    message(STATUS "Search grammer generator: '${GRAMMAR_GEN_FILENAME}'")
    find_file(GRAMMAR_GEN "${GRAMMAR_GEN_FILENAME}"
        PATHS "${GRAMMAR_MUTATOR_SOURCE_DIR}"
    )
    message(STATUS "Found grammer generator at: '${GRAMMAR_GEN}'")

    file(COPY "${GRAMMAR_GEN}" "${GRAMMAR_LIB}"
        DESTINATION "${CMAKE_CURRENT_BINARY_DIR}"
        FOLLOW_SYMLINK_CHAIN
    )
endfunction()

if(AFL_ENABLED)
    if($ENV{AFL_USE_ASAN})
        set(AFL_EXT "_asan")
        add_compile_definitions(AFL_ASAN_MODE=1)
        add_compile_options(-fno-omit-frame-pointer)
    elseif($ENV{AFL_USE_MSAN})
        set(AFL_EXT "_msan")
        add_compile_definitions(AFL_MSAN_MODE=1)
        add_compile_options(-fno-omit-frame-pointer)
    elseif($ENV{AFL_USE_UBSAN})
        set(AFL_EXT "_ubsan")
        add_compile_definitions(AFL_UBSAN_MODE=1)
        add_compile_options(-fno-omit-frame-pointer)
    elseif($ENV{AFL_USE_LSAN})
        set(AFL_EXT "_lsan")
        add_compile_definitions(AFL_LSAN_MODE=1)
        add_compile_options(-fno-omit-frame-pointer)
    else()
        unset(AFL_EXT)
        setup_grammar("grammars/civicc.json")
    endif()
endif()

