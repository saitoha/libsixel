#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Preserve more than 128 global samples across bounded GPU dispatches.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0184_gpu_selection_global_samples"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu selection global samples # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu selection global samples"
    exit 0
}
echo "ok 1 - gpu selection global samples"
exit 0
