#!/bin/sh
# Verify FHEDT fractional l low.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

unset SIXEL_LOOKUP_FHEDT_WEIGHTS
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt_weight/0020_fractional_l_low || {
    echo "not ok 1 - FHEDT fractional l low"
    exit 0
}

echo "ok 1 - FHEDT fractional l low"
exit 0
