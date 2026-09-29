#!/bin/sh
# Verify zero rate reversible p3.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/color_space/0001_space_contract \
    wire 2 0 1 hard reversible:timing=all:rate=0 || {
    echo "not ok 1 - zero rate reversible p3"
    exit 0
}
echo "ok 1 - zero rate reversible p3"
exit 0
