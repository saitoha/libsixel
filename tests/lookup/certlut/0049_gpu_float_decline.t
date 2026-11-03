#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/certlut.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" certlut/gpu_float_decline || {
    echo "not ok 1 - certlut gpu float decline"
    exit 0
}
echo "ok 1 - certlut gpu float decline"
exit 0
