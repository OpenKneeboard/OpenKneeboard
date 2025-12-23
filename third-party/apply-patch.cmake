# CMake script
# Inputs:
# - GIT_EXECUTABLE
# - PATCH_FILE
# - WORKING_DIRECTORY

message(STATUS "Checking if patch is applied...")
execute_process(
  COMMAND "${GIT_EXECUTABLE}" apply --check --reverse "${PATCH_FILE}"
  WORKING_DIRECTORY "${WORKING_DIRECTORY}"
  RESULT_VARIABLE already_applied
  OUTPUT_QUIET
  ERROR_QUIET
)
if (already_applied EQUAL 0)
  message(STATUS "Patch already applied, nothing to do.")
  return ()
endif ()

message(STATUS "Applying patch...")
execute_process(
  COMMAND "${GIT_EXECUTABLE}" apply "${PATCH_FILE}"
  WORKING_DIRECTORY "${WORKING_DIRECTORY}"
  RESULT_VARIABLE applied
)

if (applied EQUAL 0)
  message(STATUS "Patch applied.")
  return ()
endif ()

message(FATAL_ERROR "Failed to apply patch.")