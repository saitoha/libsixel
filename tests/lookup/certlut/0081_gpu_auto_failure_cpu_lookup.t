#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/certlut.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette-direct \
    certlut/gpu_auto_failure_cpu_lookup || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - certlut gpu auto failure cpu lookup"
    exit 0
}
echo "ok 1 - certlut gpu auto failure cpu lookup"
exit 0
