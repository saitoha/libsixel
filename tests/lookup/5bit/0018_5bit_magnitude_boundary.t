#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/README.md
# 5bit filtered scan magnitude boundary
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/filtered/0003_5bit_magnitude_boundary || {
    printf '\n%s\n' "not ok 1 - 5bit filtered scan magnitude boundary"
    exit 0
}
printf '\n%s\n' "ok 1 - 5bit filtered scan magnitude boundary"
exit 0
