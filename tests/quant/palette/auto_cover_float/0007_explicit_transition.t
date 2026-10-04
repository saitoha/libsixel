#!/bin/sh
# Policy: docs/encoder/palette-construction/cover-policy/README.md
# Explicit corners still grow between two AUTO calls.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/auto_cover_float/0007_explicit_transition || {
    echo "not ok 1 - Explicit corners still grow between two AUTO calls"
    exit 0
}
echo "ok 1 - Explicit corners still grow between two AUTO calls"
exit 0
