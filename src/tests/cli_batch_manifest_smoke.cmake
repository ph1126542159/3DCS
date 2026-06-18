if(NOT DEFINED CLI_EXE)
  message(FATAL_ERROR "CLI_EXE is required")
endif()
if(NOT DEFINED MODEL_A_PATH)
  message(FATAL_ERROR "MODEL_A_PATH is required")
endif()
if(NOT DEFINED MODEL_B_PATH)
  message(FATAL_ERROR "MODEL_B_PATH is required")
endif()
if(NOT DEFINED BATCH_LIST_PATH)
  message(FATAL_ERROR "BATCH_LIST_PATH is required")
endif()
if(NOT DEFINED BATCH_OUT_DIR)
  message(FATAL_ERROR "BATCH_OUT_DIR is required")
endif()

file(REMOVE "${MODEL_A_PATH}" "${MODEL_B_PATH}" "${BATCH_LIST_PATH}")
file(REMOVE_RECURSE "${BATCH_OUT_DIR}")

foreach(model_path IN ITEMS "${MODEL_A_PATH}" "${MODEL_B_PATH}")
  execute_process(
    COMMAND "${CLI_EXE}" --starter-smoke --model-out "${model_path}"
            --runs 8 --seed 12345 --out "${model_path}.html"
    RESULT_VARIABLE seed_rc
    OUTPUT_VARIABLE seed_out
    ERROR_VARIABLE seed_err)

  if(NOT seed_rc EQUAL 0)
    message(FATAL_ERROR "Could not create seed model ${model_path} with ${seed_rc}\n${seed_out}\n${seed_err}")
  endif()
  file(REMOVE "${model_path}.html")
endforeach()

file(WRITE "${BATCH_LIST_PATH}"
     "${MODEL_A_PATH},16,111,1\n${MODEL_B_PATH},24,222,2\n")

execute_process(
  COMMAND "${CLI_EXE}" --batch-list "${BATCH_LIST_PATH}"
          --batch-out-dir "${BATCH_OUT_DIR}"
          --runs 32 --seed 12345 --threads 4
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err)

if(NOT rc EQUAL 0)
  message(FATAL_ERROR "CLI batch manifest smoke failed with ${rc}\n${out}\n${err}")
endif()

foreach(text IN ITEMS "Batch item 1/2" "16 runs, seed 111, threads 1"
                      "Batch item 2/2" "24 runs, seed 222, threads 2"
                      "Batch completed: 2 succeeded, 0 failed")
  string(FIND "${out}" "${text}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "CLI batch manifest output did not contain '${text}'\n${out}\n${err}")
  endif()
endforeach()

foreach(path IN ITEMS "${BATCH_OUT_DIR}/batch-1.html"
                      "${BATCH_OUT_DIR}/batch-1.csv"
                      "${BATCH_OUT_DIR}/batch-1.xml.xls"
                      "${BATCH_OUT_DIR}/batch-1.hst"
                      "${BATCH_OUT_DIR}/batch-1.hlm"
                      "${BATCH_OUT_DIR}/batch-2.html"
                      "${BATCH_OUT_DIR}/batch-2.csv"
                      "${BATCH_OUT_DIR}/batch-2.xml.xls"
                      "${BATCH_OUT_DIR}/batch-2.hst"
                      "${BATCH_OUT_DIR}/batch-2.hlm")
  if(NOT EXISTS "${path}")
    message(FATAL_ERROR "Expected batch manifest report was not written: ${path}")
  endif()
endforeach()

file(REMOVE "${MODEL_A_PATH}" "${MODEL_B_PATH}" "${BATCH_LIST_PATH}")
file(REMOVE_RECURSE "${BATCH_OUT_DIR}")
