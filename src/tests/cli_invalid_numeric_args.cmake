if(NOT DEFINED CLI_EXE)
  message(FATAL_ERROR "CLI_EXE is required")
endif()
if(NOT DEFINED REPORT_PATH)
  message(FATAL_ERROR "REPORT_PATH is required")
endif()

function(expect_invalid_numeric_arg arg_name arg_value expected_error)
  file(REMOVE "${REPORT_PATH}")

  execute_process(
    COMMAND "${CLI_EXE}" "${arg_name}" "${arg_value}" --out "${REPORT_PATH}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)

  if(rc EQUAL 0)
    message(FATAL_ERROR "CLI accepted malformed numeric argument ${arg_name}\n${out}\n${err}")
  endif()

  string(FIND "${err}" "${expected_error}" error_pos)
  if(error_pos EQUAL -1)
    message(FATAL_ERROR "CLI did not explain malformed ${arg_name}\n${out}\n${err}")
  endif()

  if(EXISTS "${REPORT_PATH}")
    message(FATAL_ERROR "CLI wrote report after malformed numeric argument ${arg_name}")
  endif()
endfunction()

function(expect_invalid_cli_args expected_error)
  file(REMOVE "${REPORT_PATH}")

  execute_process(
    COMMAND "${CLI_EXE}" ${ARGN} --out "${REPORT_PATH}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)

  if(rc EQUAL 0)
    message(FATAL_ERROR "CLI accepted invalid arguments: ${ARGN}\n${out}\n${err}")
  endif()

  string(FIND "${err}" "${expected_error}" error_pos)
  if(error_pos EQUAL -1)
    message(FATAL_ERROR "CLI did not explain invalid arguments: ${ARGN}\n${out}\n${err}")
  endif()

  if(EXISTS "${REPORT_PATH}")
    message(FATAL_ERROR "CLI wrote report after invalid arguments: ${ARGN}")
  endif()
endfunction()

expect_invalid_numeric_arg("--runs" "12oops" "Invalid --runs value")
expect_invalid_numeric_arg("--seed" "99oops" "Invalid --seed value")
expect_invalid_numeric_arg("--threads" "2oops" "Invalid --threads value")
expect_invalid_cli_args("Missing value for --runs" "--runs")
expect_invalid_cli_args("Missing value for --runs" "--runs" "--seed" "12345")
expect_invalid_cli_args("Unknown argument: --definitely-unknown" "--definitely-unknown")
