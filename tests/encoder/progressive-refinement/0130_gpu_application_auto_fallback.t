#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Retry a failed GPU batch on CPU while accounting for its work budget.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0181_gpu_application_auto_fallback"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu application auto fallback # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu application auto fallback"
    exit 0
}
echo "ok 1 - gpu application auto fallback"
exit 0
