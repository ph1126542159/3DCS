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

file(WRITE "${BATCH_LIST_PATH}" "${MODEL_A_PATH}\n${MODEL_B_PATH}\n")

execute_process(
  COMMAND "${CLI_EXE}" --batch-list "${BATCH_LIST_PATH}"
          --batch-out-dir "${BATCH_OUT_DIR}"
          --runs 32 --seed 12345 --threads 2
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err)

if(NOT rc EQUAL 0)
  message(FATAL_ERROR "CLI batch smoke failed with ${rc}\n${out}\n${err}")
endif()

foreach(text IN ITEMS "Batch item 1/2" "Batch item 2/2"
                      "Batch completed: 2 succeeded, 0 failed"
                      "threads 2")
  string(FIND "${out}" "${text}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "CLI batch output did not contain '${text}'\n${out}\n${err}")
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
    message(FATAL_ERROR "Expected batch report was not written: ${path}")
  endif()
endforeach()

foreach(path IN ITEMS "${BATCH_OUT_DIR}/batch-1.html"
                      "${BATCH_OUT_DIR}/batch-2.html")
  file(READ "${path}" report)
  string(FIND "${report}" "PointPoint_401" measure_pos)
  if(measure_pos EQUAL -1)
    message(FATAL_ERROR "Batch report did not contain PointPoint_401: ${path}")
  endif()
endforeach()

foreach(path IN ITEMS "${BATCH_OUT_DIR}/batch-1.csv"
                      "${BATCH_OUT_DIR}/batch-2.csv")
  file(READ "${path}" csv)
  string(FIND "${csv}" "Measure,Nominal,Mean,Sigma,6Sigma,Min,Max,Cp,Cpk,TotOUT%,DPMO" csv_header_pos)
  if(csv_header_pos EQUAL -1)
    message(FATAL_ERROR "Batch CSV did not contain the statistics header: ${path}")
  endif()
endforeach()

foreach(path IN ITEMS "${BATCH_OUT_DIR}/batch-1.xml.xls"
                      "${BATCH_OUT_DIR}/batch-2.xml.xls")
  file(READ "${path}" excel)
  string(FIND "${excel}" "<Workbook" workbook_pos)
  if(workbook_pos EQUAL -1)
    message(FATAL_ERROR "Batch Excel XML did not contain Workbook root: ${path}")
  endif()
endforeach()

foreach(path IN ITEMS "${BATCH_OUT_DIR}/batch-1.hst"
                      "${BATCH_OUT_DIR}/batch-2.hst")
  file(READ "${path}" hst)
  string(FIND "${hst}" "Build,M401" hst_header_pos)
  if(hst_header_pos EQUAL -1)
    message(FATAL_ERROR "Batch HST did not contain the sample header: ${path}")
  endif()
endforeach()

foreach(path IN ITEMS "${BATCH_OUT_DIR}/batch-1.hlm"
                      "${BATCH_OUT_DIR}/batch-2.hlm")
  file(READ "${path}" hlm)
  string(FIND "${hlm}" "Measure,Contributor,GeoFactor,6Sigma,ContributionPct" hlm_header_pos)
  if(hlm_header_pos EQUAL -1)
    message(FATAL_ERROR "Batch HLM did not contain the contributor header: ${path}")
  endif()
endforeach()

file(REMOVE "${MODEL_A_PATH}" "${MODEL_B_PATH}" "${BATCH_LIST_PATH}")
file(REMOVE_RECURSE "${BATCH_OUT_DIR}")
