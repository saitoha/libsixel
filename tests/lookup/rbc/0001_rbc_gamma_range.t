#!/bin/sh
# Verify rbc gamma range.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/extended/0001_extended_lookup range 4 0 || {
    printf '\n%s\n' "not ok 1 - rbc gamma range"
    exit 0
}

printf '\n%s\n' "ok 1 - rbc gamma range"
exit 0
