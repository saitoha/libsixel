#!/bin/sh
# Verify wire hard cielab p3.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/color_space/0001_space_contract \
    wire 3 0 1 hard || {
    echo "not ok 1 - wire hard cielab p3"
    exit 0
}
echo "ok 1 - wire hard cielab p3"
exit 0
