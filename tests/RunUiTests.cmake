# Preserve QtTest diagnostics even on Windows hosts without a console for Qt logging.
execute_process(COMMAND "${TEST_EXE}" -platform offscreen -o "${LOG_PATH},txt"
  RESULT_VARIABLE result TIMEOUT 100)
if(EXISTS "${LOG_PATH}")
  file(READ "${LOG_PATH}" output)
  message("${output}")
endif()
if(NOT result STREQUAL "0")
  message(FATAL_ERROR "Qt integration tests failed: ${result}")
endif()
