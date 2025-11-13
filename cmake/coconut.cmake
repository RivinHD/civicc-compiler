Include(FetchContent)
FetchContent_Declare(
    coconut
    GIT_REPOSITORY https://github.com/CoCoNut-UvA/coconut
    GIT_TAG        2ac7ef0adedcfc318c5225eae7942a08445db194
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(coconut)

FetchContent_GetProperties(coconut
    SOURCE_DIR COCONUT_ROOT_DIR 
    BINARY_DIR COCONUT_BUILD_DIR
)

if(NOT DEFINED ${COCOGEN_DIR})
    set(COCOGEN_DIR "${COCONUT_BUILD_DIR}/cocogen")
endif()


if(NOT EXISTS "${COCOGEN_DIR}/cocogen")
    include(ProcessorCount)
    ProcessorCount(CPU_COUNT)

    message(STATUS "Build the cocogen executable")
    message(STATUS "Parallel Build: ${CPU_COUNT} cores")
    execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" -B "${COCONUT_BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
        WORKING_DIRECTORY "${COCONUT_ROOT_DIR}")
    execute_process(COMMAND ${CMAKE_COMMAND} --build "${COCONUT_BUILD_DIR}" --config Release --parallel ${CPU_COUNT} --target cocogen
        WORKING_DIRECTORY "${COCONUT_ROOT_DIR}")
else()
    message(STATUS "Already build.")
endif()


# We need to check if the DSL file is actually present.
function(cocogen_do_generate DSL_FILE BACKEND)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${DSL_FILE}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_BINARY_DIR}/ccngen/")
    if (NOT EXISTS "${COCOGEN_DIR}/cocogen")
        message(FATAL_ERROR "Could not find the cocogen executable in path: ${COCOGEN_DIR}
Maybe you forgot building the coconut project?")
    endif()

    message(STATUS "Generating with cocogen
    ") # Forces newline
    execute_process(COMMAND "${COCOGEN_DIR}/cocogen" "--backend" "${BACKEND}" "${DSL_FILE}"
        RESULT_VARIABLE COCOGEN_RET
        WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
        INPUT_FILE "${DSL_FILE}"
    )
    if(COCOGEN_RET AND NOT COCOGEN_RET EQUAL 0)
        message(FATAL_ERROR "cocogen generation failed (${COCOGEN_RET}), stopping CMake.")
    endif()
endfunction()

function(coconut_target_generate TARGET DSL_FILE BACKEND)
    cocogen_do_generate("${DSL_FILE}" "${BACKEND}")
    set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${CMAKE_CURRENT_BINARY_DIR}/ccngen")
    file(GLOB COCONUT_SOURCES "${PROJECT_BINARY_DIR}/ccngen/*.c" "${COCONUT_ROOT_DIR}/copra/src/*.c")
    target_sources("${TARGET}" PRIVATE "${COCONUT_SOURCES}")
    target_include_directories("${TARGET}" PRIVATE "${PROJECT_BINARY_DIR}/ccngen/" "${COCONUT_ROOT_DIR}/copra/")
    target_link_libraries("${TARGET}" PRIVATE coconut::palm)
endfunction()
