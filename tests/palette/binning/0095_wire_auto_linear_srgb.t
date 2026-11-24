#!/bin/sh
# Verify wire auto linear srgb.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/color_space/0001_space_contract \
    wire 1 0 0 auto || {
    printf '\n%s\n' "not ok 1 - wire auto linear srgb"
    exit 0
}
printf '\n%s\n' "ok 1 - wire auto linear srgb"
exit 0
