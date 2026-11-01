#!/bin/sh
# Policy: docs/encoder/palette-construction/cover-policy/README.md
# AUTO preserves typed linear coordinates without a round trip.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/auto_cover_float/0006_linear_noop || {
    echo "not ok 1 - AUTO preserves typed linear coordinates without a round trip"
    exit 0
}
echo "ok 1 - AUTO preserves typed linear coordinates without a round trip"
exit 0
