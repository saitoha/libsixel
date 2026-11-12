#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/rbc.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette-direct rbc/gpu_exact_4_0_33 || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - rbc gpu exact 4 0 33"
    exit 0
}
echo "ok 1 - rbc gpu exact 4 0 33"
exit 0
