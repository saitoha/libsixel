#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/blockcull.md
# Verify blockcull cielab range.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/blockcull/0004_blockcull_cielab_range || {
    echo "not ok 1 - blockcull cielab range"
    exit 0
}
echo "ok 1 - blockcull cielab range"
exit 0
