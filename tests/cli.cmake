execute_process(
    COMMAND "${PROGRAM}" verify "${FIXTURES}/delayed_controller.csv"
    RESULT_VARIABLE result OUTPUT_VARIABLE output)
if(NOT result EQUAL 1 OR NOT output MATCHES "expected_stop=12 observed_stop=13")
    message(FATAL_ERROR "Late-stop replay did not return its expected diagnostic")
endif()

execute_process(
    COMMAND "${PROGRAM}" verify "${FIXTURES}/missing.csv"
    RESULT_VARIABLE result ERROR_QUIET)
if(NOT result EQUAL 2)
    message(FATAL_ERROR "Missing input did not return exit code 2")
endif()

execute_process(
    COMMAND "${PROGRAM}" jam --fault-tick invalid
    RESULT_VARIABLE result ERROR_QUIET)
if(NOT result EQUAL 2)
    message(FATAL_ERROR "Invalid fault time did not return exit code 2")
endif()

foreach(scenario forward reversal arrival limits jam frozen slow)
    execute_process(COMMAND "${PROGRAM}" "${scenario}"
        RESULT_VARIABLE first_result OUTPUT_VARIABLE first)
    execute_process(COMMAND "${PROGRAM}" "${scenario}"
        RESULT_VARIABLE second_result OUTPUT_VARIABLE second)
    if(NOT first_result EQUAL 0 OR NOT second_result EQUAL 0 OR NOT first STREQUAL second)
        message(FATAL_ERROR "${scenario} changed between separate processes")
    endif()
endforeach()

execute_process(
    COMMAND "${PROGRAM}" frozen --fault-tick 20 --ticks 64
    RESULT_VARIABLE result OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/fault20.csv")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Configured fault scenario failed")
endif()
execute_process(
    COMMAND "${PROGRAM}" verify "${CMAKE_CURRENT_BINARY_DIR}/fault20.csv"
    RESULT_VARIABLE result OUTPUT_QUIET)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Configured fault trace failed verification")
endif()

foreach(speed 99 1001 0 invalid 2147483648)
    execute_process(
        COMMAND "${PROGRAM}" jam --speed "${speed}"
        RESULT_VARIABLE result ERROR_QUIET)
    if(NOT result EQUAL 2)
        message(FATAL_ERROR "Unsupported speed ${speed} did not return exit code 2")
    endif()
endforeach()

execute_process(
    COMMAND "${PROGRAM}" jam --fault-tick 7 --ticks 32 --speed 100
    RESULT_VARIABLE result
    OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/slow_jam.csv")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Slow jam scenario failed")
endif()
file(READ "${CMAKE_CURRENT_BINARY_DIR}/slow_jam.csv" slow_trace)
if(NOT slow_trace MATCHES "17,80,1,0,700,0,1")
    message(FATAL_ERROR "Slow jam did not stop on tick 17")
endif()
execute_process(
    COMMAND "${PROGRAM}" verify "${CMAKE_CURRENT_BINARY_DIR}/slow_jam.csv"
    RESULT_VARIABLE result OUTPUT_QUIET)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Slow jam trace failed verification")
endif()

execute_process(
    COMMAND "${PROGRAM}" jam --fault-tick 0 --ticks 1
    RESULT_VARIABLE result OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/short.csv")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "One-sample trace failed")
endif()
execute_process(
    COMMAND "${PROGRAM}" verify "${CMAKE_CURRENT_BINARY_DIR}/short.csv"
    RESULT_VARIABLE result OUTPUT_VARIABLE output)
if(NOT result EQUAL 0 OR NOT output MATCHES "checked window only")
    message(FATAL_ERROR "Replay did not identify the scope of its short recording")
endif()

foreach(scenario jam frozen)
    execute_process(
        COMMAND "${PROGRAM}" "${scenario}" --ticks 32
        RESULT_VARIABLE result
        OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/example_${scenario}.csv")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Example generation failed")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${CMAKE_CURRENT_BINARY_DIR}/example_${scenario}.csv"
            "${EXAMPLES}/${scenario}.csv"
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Committed ${scenario} example is stale")
    endif()
endforeach()
