#!/bin/sh
# Verify eytzinger overflow.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/eytzinger/0013_eyt_overflow || {
    printf '\n%s\n' "not ok 1 - eytzinger overflow"
    exit 0
}

printf '\n%s\n' "ok 1 - eytzinger overflow"
exit 0
