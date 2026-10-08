#!/bin/sh
# Byte hooks retain bounded integer safe-tone output.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/snap_hooks/byte_grid" || {
    echo "not ok 1 - Byte hooks retain bounded integer safe-tone output"
    exit 0
}
echo "ok 1 - Byte hooks retain bounded integer safe-tone output"
exit 0
