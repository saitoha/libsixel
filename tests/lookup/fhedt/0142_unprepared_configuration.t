#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/fhedt.md
# Verify unprepared configuration.
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    fhedt/lookup_configuration 2 || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - unprepared configuration"
    exit 0
}
echo "ok 1 - unprepared configuration"
exit 0
