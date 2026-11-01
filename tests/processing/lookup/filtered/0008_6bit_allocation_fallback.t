#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/README.md
# 6bit filtered scan allocation fallback
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/filtered/0008_6bit_allocation_fallback || {
    echo "not ok 1 - 6bit filtered scan allocation fallback"
    exit 0
}
echo "ok 1 - 6bit filtered scan allocation fallback"
exit 0
