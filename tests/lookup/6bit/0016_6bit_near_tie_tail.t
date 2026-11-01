#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/README.md
# 6bit filtered scan near tie tail
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/filtered/0005_6bit_near_tie_tail || {
    echo "not ok 1 - 6bit filtered scan near tie tail"
    exit 0
}
echo "ok 1 - 6bit filtered scan near tie tail"
exit 0
