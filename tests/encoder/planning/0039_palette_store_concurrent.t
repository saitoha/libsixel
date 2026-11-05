#!/bin/sh
# Verify concurrent palette store publication and selection.
# Policy: docs/threading/encoder.md
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled"
    exit 0
}

echo "1..1"
set -v

result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0039_palette_store_concurrent || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP asynchronous palette scheduling is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - concurrent palette store publication and selection"
    exit 0
}
echo "ok 1 - concurrent palette store publication and selection"
exit 0
