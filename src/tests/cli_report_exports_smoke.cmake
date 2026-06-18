if(NOT DEFINED CLI_EXE)
  message(FATAL_ERROR "CLI_EXE is required")
endif()
if(NOT DEFINED MODEL_PATH)
  message(FATAL_ERROR "MODEL_PATH is required")
endif()
if(NOT DEFINED HTML_PATH)
  message(FATAL_ERROR "HTML_PATH is required")
endif()
if(NOT DEFINED CSV_PATH)
  message(FATAL_ERROR "CSV_PATH is required")
endif()
if(NOT DEFINED EXCEL_PATH)
  message(FATAL_ERROR "EXCEL_PATH is required")
endif()
if(NOT DEFINED HST_PATH)
  message(FATAL_ERROR "HST_PATH is required")
endif()
if(NOT DEFINED HLM_PATH)
  message(FATAL_ERROR "HLM_PATH is required")
endif()

file(REMOVE "${MODEL_PATH}" "${HTML_PATH}" "${CSV_PATH}" "${EXCEL_PATH}"
            "${HST_PATH}" "${HLM_PATH}")

execute_process(
  COMMAND "${CLI_EXE}" --starter-smoke --model-out "${MODEL_PATH}"
          --runs 8 --seed 12345 --out "${HTML_PATH}.seed"
  RESULT_VARIABLE seed_rc
  OUTPUT_VARIABLE seed_out
  ERROR_VARIABLE seed_err)

if(NOT seed_rc EQUAL 0)
  message(FATAL_ERROR "Could not create seed model with ${seed_rc}\n${seed_out}\n${seed_err}")
endif()

file(REMOVE "${HTML_PATH}.seed")

execute_process(
  COMMAND "${CLI_EXE}" --model-in "${MODEL_PATH}"
          --runs 64 --seed 12345 --threads 4 --out "${HTML_PATH}"
          --csv-out "${CSV_PATH}" --excel-out "${EXCEL_PATH}"
          --hst-out "${HST_PATH}" --hlm-out "${HLM_PATH}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err)

if(NOT rc EQUAL 0)
  message(FATAL_ERROR "CLI report export smoke failed with ${rc}\n${out}\n${err}")
endif()
string(FIND "${out}" "threads 4" threads_pos)
if(threads_pos EQUAL -1)
  message(FATAL_ERROR "CLI report export did not report the requested thread count\n${out}\n${err}")
endif()

foreach(path IN ITEMS "${HTML_PATH}" "${CSV_PATH}" "${EXCEL_PATH}"
                      "${HST_PATH}" "${HLM_PATH}")
  if(NOT EXISTS "${path}")
    message(FATAL_ERROR "Expected report export was not written: ${path}")
  endif()
endforeach()

file(READ "${CSV_PATH}" csv)
string(FIND "${csv}" "Measure,Nominal,Mean,Sigma,6Sigma,Min,Max,Cp,Cpk,TotOUT%,DPMO" csv_header_pos)
if(csv_header_pos EQUAL -1)
  message(FATAL_ERROR "CSV report did not contain the statistics header")
endif()
string(FIND "${csv}" "PointPoint_401" csv_measure_pos)
if(csv_measure_pos EQUAL -1)
  message(FATAL_ERROR "CSV report did not contain PointPoint_401")
endif()

file(READ "${EXCEL_PATH}" excel)
string(FIND "${excel}" "<Workbook" workbook_pos)
if(workbook_pos EQUAL -1)
  message(FATAL_ERROR "Excel XML report did not contain Workbook root")
endif()
string(FIND "${excel}" "PointPoint_401" excel_measure_pos)
if(excel_measure_pos EQUAL -1)
  message(FATAL_ERROR "Excel XML report did not contain PointPoint_401")
endif()

file(READ "${HST_PATH}" hst)
string(FIND "${hst}" "Build,M401" hst_header_pos)
if(hst_header_pos EQUAL -1)
  message(FATAL_ERROR "HST file did not contain the sample header")
endif()

file(READ "${HLM_PATH}" hlm)
string(FIND "${hlm}" "Measure,Contributor,GeoFactor,6Sigma,ContributionPct" hlm_header_pos)
if(hlm_header_pos EQUAL -1)
  message(FATAL_ERROR "HLM file did not contain the contributor header")
endif()

file(REMOVE "${MODEL_PATH}" "${HTML_PATH}" "${CSV_PATH}" "${EXCEL_PATH}"
            "${HST_PATH}" "${HLM_PATH}")
