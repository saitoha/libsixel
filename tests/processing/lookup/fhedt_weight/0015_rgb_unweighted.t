#!/bin/sh
# Verify FHEDT rgb unweighted.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

unset SIXEL_LOOKUP_FHEDT_WEIGHTS
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt_weight/0015_rgb_unweighted || {
    echo "not ok 1 - FHEDT rgb unweighted"
    exit 0
}

echo "ok 1 - FHEDT rgb unweighted"
exit 0
