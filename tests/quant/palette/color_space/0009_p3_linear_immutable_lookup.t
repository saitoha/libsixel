#!/bin/sh
# p3 linear immutable lookup.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/color_space/0001_space_contract \
    lookup 1 0 1 || {
    echo "not ok 1 - p3 linear immutable lookup"
    exit 0
}
echo "ok 1 - p3 linear immutable lookup"
exit 0
