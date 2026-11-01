#!/bin/sh
# srgb X linear W gamma wire.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/color_space/0001_space_contract \
    wire 1 0 0 || {
    echo "not ok 1 - srgb X linear W gamma wire"
    exit 0
}
echo "ok 1 - srgb X linear W gamma wire"
exit 0
