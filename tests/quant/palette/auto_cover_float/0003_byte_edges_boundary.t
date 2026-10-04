#!/bin/sh
# Policy: docs/encoder/palette-construction/cover-policy/README.md
# 256 colors do not inherit the byte edges budget.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/auto_cover_float/0003_byte_edges_boundary || {
    echo "not ok 1 - 256 colors do not inherit the byte edges budget"
    exit 0
}
echo "ok 1 - 256 colors do not inherit the byte edges budget"
exit 0
