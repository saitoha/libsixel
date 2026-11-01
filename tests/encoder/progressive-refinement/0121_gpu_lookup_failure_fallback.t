#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Preserve output on failure, allow AUTO fallback and recover.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0171_gpu_lookup_failure_fallback"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu lookup failure fallback # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu lookup failure fallback"
    exit 0
}
echo "ok 1 - gpu lookup failure fallback"
exit 0
