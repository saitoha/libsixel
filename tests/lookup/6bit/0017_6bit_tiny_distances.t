#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/README.md
# 6bit filtered scan tiny distances
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/filtered/0006_6bit_tiny_distances || {
    echo "not ok 1 - 6bit filtered scan tiny distances"
    exit 0
}
echo "ok 1 - 6bit filtered scan tiny distances"
exit 0
