# Applies PATCH_FILE to the current directory (the fetched ysfx checkout).
# Idempotent: does nothing if the patch is already applied.
find_package(Git REQUIRED)
execute_process(COMMAND "${GIT_EXECUTABLE}" apply --reverse --check --ignore-whitespace "${PATCH_FILE}"
                RESULT_VARIABLE already OUTPUT_QUIET ERROR_QUIET)
if(already EQUAL 0)
    message(STATUS "ysfx patch already applied")
    return()
endif()
execute_process(COMMAND "${GIT_EXECUTABLE}" apply --ignore-whitespace "${PATCH_FILE}"
                RESULT_VARIABLE res)
if(NOT res EQUAL 0)
    message(FATAL_ERROR "Failed to apply ${PATCH_FILE}")
endif()
message(STATUS "Applied ${PATCH_FILE}")
