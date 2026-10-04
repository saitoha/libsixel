#!/bin/sh
# Policy: docs/encoder/palette-construction/cover-policy/README.md
# 32 colors retain exact float entries without histogram allocation.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/auto_cover_float/0001_byte_corners_boundary || {
    echo "not ok 1 - 32 colors retain exact float entries without histogram allocation"
    exit 0
}
echo "ok 1 - 32 colors retain exact float entries without histogram allocation"
exit 0
