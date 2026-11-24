#!/bin/sh
# Verify P3 snap full allocation recovery.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/p3_snap/0001_grid failure 1 || {
    printf '\n%s\n' "not ok 1 - full allocation recovery"
    exit 0
}
printf '\n%s\n' "ok 1 - full allocation recovery"
exit 0
