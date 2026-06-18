if(NOT DEFINED CLI_EXE)
  message(FATAL_ERROR "CLI_EXE is required")
endif()
if(NOT DEFINED MODEL_PATH)
  message(FATAL_ERROR "MODEL_PATH is required")
endif()
if(NOT DEFINED REPORT_PATH)
  message(FATAL_ERROR "REPORT_PATH is required")
endif()

file(REMOVE "${MODEL_PATH}" "${REPORT_PATH}")

execute_process(
  COMMAND "${CLI_EXE}" --starter-smoke --model-out "${MODEL_PATH}"
          --runs 64 --seed 12345 --out "${REPORT_PATH}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err)

if(NOT rc EQUAL 0)
  message(FATAL_ERROR "CLI starter smoke failed with ${rc}\n${out}\n${err}")
endif()

if(NOT EXISTS "${MODEL_PATH}")
  message(FATAL_ERROR "CLI starter smoke did not write model file")
endif()
if(NOT EXISTS "${REPORT_PATH}")
  message(FATAL_ERROR "CLI starter smoke did not write report file")
endif()

file(READ "${REPORT_PATH}" report)
string(FIND "${report}" "UntitledModel" title_pos)
if(title_pos EQUAL -1)
  message(FATAL_ERROR "CLI starter smoke report did not contain UntitledModel")
endif()
string(FIND "${report}" "PointPoint_401" measure_pos)
if(measure_pos EQUAL -1)
  message(FATAL_ERROR "CLI starter smoke report did not contain PointPoint_401")
endif()

file(REMOVE "${MODEL_PATH}" "${REPORT_PATH}")
