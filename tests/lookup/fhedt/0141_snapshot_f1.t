#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/fhedt.md
# Verify snapshot f1.
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    fhedt/lookup_configuration 1 || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - snapshot f1"
    exit 0
}
echo "ok 1 - snapshot f1"
exit 0
