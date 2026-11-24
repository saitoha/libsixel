#!/bin/sh
# Verify eytzinger projection overflow.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/eytzinger/0033_eyt_projection_overflow || {
    printf '\n%s\n' "not ok 1 - eytzinger projection overflow"
    exit 0
}

printf '\n%s\n' "ok 1 - eytzinger projection overflow"
exit 0
