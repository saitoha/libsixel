#!/bin/sh
# Clearing temporal history removes prior correction before the next apply.
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0040_temporal_reset" || {
    echo "not ok 1 - temporal_reset"
    exit 0
}
echo "ok 1 - temporal_reset"
exit 0
