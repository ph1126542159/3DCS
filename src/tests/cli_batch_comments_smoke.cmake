execute_process(
  COMMAND "${CLI_EXE}" --starter-smoke --model-out "${MODEL_A_PATH}"
          --out "${MODEL_A_PATH}.html" --runs 16 --seed 111
  RESULT_VARIABLE rc_a
  OUTPUT_VARIABLE out_a
  ERROR_VARIABLE err_a)
if(NOT rc_a EQUAL 0)
  message(FATAL_ERROR "Could not create first batch comments model\n${out_a}\n${err_a}")
endif()

execute_process(
  COMMAND "${CLI_EXE}" --starter-smoke --model-out "${MODEL_B_PATH}"
          --out "${MODEL_B_PATH}.html" --runs 16 --seed 222
  RESULT_VARIABLE rc_b
  OUTPUT_VARIABLE out_b
  ERROR_VARIABLE err_b)
if(NOT rc_b EQUAL 0)
  message(FATAL_ERROR "Could not create second batch comments model\n${out_b}\n${err_b}")
endif()

file(WRITE "${BATCH_LIST_PATH}" "# nightly dashboard batch\n")
file(APPEND "${BATCH_LIST_PATH}" "  ${MODEL_A_PATH}  \n")
file(APPEND "${BATCH_LIST_PATH}" "\n")
file(APPEND "${BATCH_LIST_PATH}" "\t# second model follows\n")
file(APPEND "${BATCH_LIST_PATH}" "${MODEL_B_PATH}\n")

file(REMOVE_RECURSE "${BATCH_OUT_DIR}")

execute_process(
  COMMAND "${CLI_EXE}" --batch-list "${BATCH_LIST_PATH}"
          --batch-out-dir "${BATCH_OUT_DIR}" --runs 24 --seed 333
          --threads 2
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err)

if(NOT rc EQUAL 0)
  message(FATAL_ERROR "CLI batch comments smoke failed with ${rc}\n${out}\n${err}")
endif()

foreach(text IN ITEMS "Batch item 1/2" "Batch item 2/2"
                      "Batch completed: 2 succeeded, 0 failed")
  string(FIND "${out}" "${text}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "CLI batch comments output did not contain '${text}'\n${out}\n${err}")
  endif()
endforeach()

foreach(path IN ITEMS "${BATCH_OUT_DIR}/batch-1.html"
                      "${BATCH_OUT_DIR}/batch-2.html")
  if(NOT EXISTS "${path}")
    message(FATAL_ERROR "Expected batch comments report was not written: ${path}")
  endif()
endforeach()
