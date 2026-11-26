#!/bin/sh
# GPU band composition writes the same bytes as the CPU compose.
# Policy: docs/threading/encoder.md
set -eux
echo '1..1'
set -v
status=0
# Instrumented pipelines can require more threadgroup memory. The resulting
# body must still match the CPU bytes when Metal selects serial ordering.
export MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 SIXEL_TRACE_TOPIC=metal_body
# The Intel macOS CI virtual GPU can block while creating a pipeline with
# shader validation. Keep API validation and run the same GPU comparison.
test -n "${SIXEL_TEST_MACOS_PRODUCT_VERSION-}" &&
    test "${SIXEL_TEST_HOST_ARCH-}" = x86_64 &&
    export MTL_SHADER_VALIDATION=0
test -x /usr/bin/sample && test -z "${SIXEL_RUNTIME-}" && {
    "${TEST_RUNNER_PATH}" encoder/indexed/0184_core_gpu_body &
    runner_pid=$!
    (
        trap 'test -z "${delay_pid-}" || kill "$delay_pid" 2>/dev/null || :; exit 0' TERM
        sleep 60 &
        delay_pid=$!
        wait "$delay_pid"
        # Let an in-progress sample finish if the runner exits on its own.
        trap ':' TERM
        kill -0 "$runner_pid" 2>/dev/null || exit 0
        # Leave time for sample to write its report before the 90s timeout.
        mkdir -p "${ARTIFACT_LOCAL_DIR}"
        /usr/bin/sample "$runner_pid" 2 -mayDie -file \
            "${ARTIFACT_LOCAL_DIR}/test_runner.sample.txt"
    ) >&2 &
    watchdog_pid=$!
    wait "$runner_pid" || status=$?
    kill "$watchdog_pid" 2>/dev/null || :
    wait "$watchdog_pid" 2>/dev/null || :
}
test -n "${runner_pid-}" || {
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" encoder/indexed/0184_core_gpu_body || status=$?
}
test "$status" -ne 77 || {
    printf '\n%s\n' 'ok 1 # SKIP thread support unavailable'
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - core gpu body'
    exit 0
}
printf '\n%s\n' 'ok 1 - core gpu body'
exit 0
