#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify source clock preserved.
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0111_source_clock_preserved"
result=$?
set -e
test "$result" -ne 77 || {
    echo "ok 1 - source clock preserved # SKIP thread support disabled"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - source clock preserved"
    exit 0
}
echo "ok 1 - source clock preserved"
exit 0
