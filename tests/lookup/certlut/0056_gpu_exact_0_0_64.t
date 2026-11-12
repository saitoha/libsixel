#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/certlut.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup certlut certlut/gpu_exact_0_0_64 || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU certlut program unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - certlut gpu exact 0 0 64"
    exit 0
}
echo "ok 1 - certlut gpu exact 0 0 64"
exit 0
