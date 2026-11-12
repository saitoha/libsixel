#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/vptree.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup vptree \
    vptree/gpu_exact_0_1_33 || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - vptree gpu exact 0 1 33"
    exit 0
}
echo "ok 1 - vptree gpu exact 0 1 33"
exit 0
