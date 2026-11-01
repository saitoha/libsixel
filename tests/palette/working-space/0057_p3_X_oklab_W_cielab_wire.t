#!/bin/sh
# p3 X oklab W cielab wire.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/color_space/0001_space_contract \
    wire 2 3 1 || {
    echo "not ok 1 - p3 X oklab W cielab wire"
    exit 0
}
echo "ok 1 - p3 X oklab W cielab wire"
exit 0
