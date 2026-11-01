#!/bin/sh
# Verify FHEDT common scale small.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

unset SIXEL_LOOKUP_FHEDT_WEIGHTS
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt_weight/0025_common_scale_small || {
    echo "not ok 1 - FHEDT common scale small"
    exit 0
}

echo "ok 1 - FHEDT common scale small"
exit 0
