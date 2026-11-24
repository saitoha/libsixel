#!/bin/sh
# Policy: docs/encoder/palette-construction/snap-policy/README.md
# Verify the final partial snap is applied exactly once in float precision.
set -eux

echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" palette/0048_snap_partial_once || {
    printf '\n%s\n' "not ok 1 - partial snap was rounded or repeated"
    exit 0
}
printf '\n%s\n' "ok 1 - partial snap preserves one float blend"
exit 0
