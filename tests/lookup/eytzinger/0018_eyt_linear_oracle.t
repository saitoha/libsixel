#!/bin/sh
# Verify eytzinger linear oracle.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/eytzinger/0017_eyt_linear_oracle || {
    printf '\n%s\n' "not ok 1 - eytzinger linear oracle"
    exit 0
}

printf '\n%s\n' "ok 1 - eytzinger linear oracle"
exit 0
