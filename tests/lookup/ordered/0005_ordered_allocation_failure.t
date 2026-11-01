#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
# ordered allocation failure falls back and rebuilds
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette \
    gpu-palette/0030_ordered_allocation_failure || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - ordered allocation failure falls back and rebuilds"
    exit 0
}
echo "ok 1 - ordered allocation failure falls back and rebuilds"
exit 0
