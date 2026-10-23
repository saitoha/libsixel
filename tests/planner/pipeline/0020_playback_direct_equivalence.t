#!/bin/sh
# Policy: docs/threading/encoder.md
# Verify playback output matches direct frame encoding.
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "planner/pipeline/0020_playback_direct_equivalence"
result=$?
set -e
test "$result" -ne 77 || {
    echo "ok 1 - playback output matches direct frame encoding # SKIP thread support disabled"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - playback output matches direct frame encoding"
    exit 0
}
echo "ok 1 - playback output matches direct frame encoding"
exit 0
