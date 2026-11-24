#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/README.md
# 6bit filtered scan allocation fallback
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/filtered/0008_6bit_allocation_fallback || {
    printf '\n%s\n' "not ok 1 - 6bit filtered scan allocation fallback"
    exit 0
}
printf '\n%s\n' "ok 1 - 6bit filtered scan allocation fallback"
exit 0
