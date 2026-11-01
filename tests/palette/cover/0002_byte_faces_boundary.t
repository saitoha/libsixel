#!/bin/sh
# Policy: docs/encoder/palette-construction/cover-policy/README.md
# 64 colors do not inherit the byte faces budget.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/auto_cover_float/0002_byte_faces_boundary || {
    echo "not ok 1 - 64 colors do not inherit the byte faces budget"
    exit 0
}
echo "ok 1 - 64 colors do not inherit the byte faces budget"
exit 0
