#!/bin/sh
# Verify rbc underflow.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/extended/0001_extended_lookup underflow 4 0 || {
    printf '\n%s\n' "not ok 1 - rbc underflow"
    exit 0
}

printf '\n%s\n' "ok 1 - rbc underflow"
exit 0
