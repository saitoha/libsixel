#!/bin/sh
# Verify fhedt outside extreme.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt/0018_fh_outside_extreme || {
    echo "not ok 1 - fhedt outside extreme"
    exit 0
}

echo "ok 1 - fhedt outside extreme"
exit 0
