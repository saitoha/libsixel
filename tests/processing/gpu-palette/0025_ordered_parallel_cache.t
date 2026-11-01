#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
# Concurrent callers with independent palettes preserve exact results.
set -eux

echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette \
    gpu-palette/0025_ordered_parallel_cache || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - ordered parallel cache"
    exit 0
}
echo "ok 1 - ordered parallel cache"
exit 0
