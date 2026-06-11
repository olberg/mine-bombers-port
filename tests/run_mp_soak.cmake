# Seeded full-match soak (CTest driver).
#
# Runs the real game executable in autoplay mode (MB_AUTOPLAY=1):
#   1. 5-round 4-bot match, seed 42  -> mp_soak_run1.jsonl
#   2. same again                    -> mp_soak_run2.jsonl
#      The two traces must be byte-identical (determinism gate — if this
#      fails, seeded original-vs-port comparison is invalid everywhere).
#   3. 25-round match, seed 42      -> mp_soak_long.jsonl
#      Must complete (results reached, exit 0) with 25 round records.
# The exe exits non-zero on any harness assertion failure, including the
# entity-allocation leak counter checked in autoplay_finish().
#
# Invoked from tests/CMakeLists.txt as:
#   cmake -DGAME_EXE=<exe> -DWORK_DIR=<exe dir> -P run_mp_soak.cmake

if(NOT GAME_EXE OR NOT WORK_DIR)
    message(FATAL_ERROR "GAME_EXE and WORK_DIR must be defined")
endif()

set(SOAK_SEED 42)

function(run_match trace_name rounds)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env
            MB_AUTOPLAY=1
            MB_SEED=${SOAK_SEED}
            MB_AUTOPLAY_ROUNDS=${rounds}
            MB_AUTOPLAY_SHOTS=0
            MB_TRACE=${trace_name}
            ${GAME_EXE}
        WORKING_DIRECTORY ${WORK_DIR}
        RESULT_VARIABLE rc
    )
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR
            "soak: ${rounds}-round match failed (exit ${rc}); "
            "see ${WORK_DIR}/${trace_name}")
    endif()
endfunction()

function(check_trace trace_name expected_rounds)
    file(STRINGS ${WORK_DIR}/${trace_name} round_lines
         REGEX "\"type\":\"round\"")
    list(LENGTH round_lines n)
    if(NOT n EQUAL ${expected_rounds})
        message(FATAL_ERROR
            "soak: ${trace_name} has ${n} round records, "
            "expected ${expected_rounds}")
    endif()
    file(STRINGS ${WORK_DIR}/${trace_name} done_lines
         REGEX "\"completed\":true")
    list(LENGTH done_lines n)
    if(NOT n EQUAL 1)
        message(FATAL_ERROR "soak: ${trace_name} lacks completed:true")
    endif()
endfunction()

message(STATUS "soak: run 1/3 — 5 rounds, seed ${SOAK_SEED}")
run_match(mp_soak_run1.jsonl 5)
check_trace(mp_soak_run1.jsonl 5)

message(STATUS "soak: run 2/3 — 5 rounds, seed ${SOAK_SEED} (determinism)")
run_match(mp_soak_run2.jsonl 5)

execute_process(
    COMMAND ${CMAKE_COMMAND} -E compare_files
        ${WORK_DIR}/mp_soak_run1.jsonl ${WORK_DIR}/mp_soak_run2.jsonl
    RESULT_VARIABLE diff_rc
)
if(NOT diff_rc EQUAL 0)
    message(FATAL_ERROR
        "soak: NONDETERMINISM — same-seed traces differ "
        "(mp_soak_run1.jsonl vs mp_soak_run2.jsonl)")
endif()

message(STATUS "soak: run 3/3 — 25 rounds, seed ${SOAK_SEED} (completion)")
run_match(mp_soak_long.jsonl 25)
check_trace(mp_soak_long.jsonl 25)

message(STATUS "soak: all checks passed")
