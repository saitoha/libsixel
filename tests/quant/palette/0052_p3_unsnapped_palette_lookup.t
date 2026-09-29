#!/bin/sh
# Verify p3 unsnapped palette lookup.
# Policy: docs/encoder/output/output-colorspace.md
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/0051_p3_palette_contract lookup || {
    echo "not ok 1 - p3 unsnapped palette lookup"
    exit 0
}

echo "ok 1 - p3 unsnapped palette lookup"
exit 0
