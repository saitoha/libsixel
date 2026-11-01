#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/README.md
# 6bit filtered scan magnitude boundary
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/filtered/0007_6bit_magnitude_boundary || {
    echo "not ok 1 - 6bit filtered scan magnitude boundary"
    exit 0
}
echo "ok 1 - 6bit filtered scan magnitude boundary"
exit 0
