#!/bin/sh
# srgb gamma immutable lookup.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/color_space/0001_space_contract \
    lookup 0 0 0 || {
    echo "not ok 1 - srgb gamma immutable lookup"
    exit 0
}
echo "ok 1 - srgb gamma immutable lookup"
exit 0
