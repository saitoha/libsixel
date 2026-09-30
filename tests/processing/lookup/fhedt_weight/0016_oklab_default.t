#!/bin/sh
# Verify FHEDT oklab default.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

unset SIXEL_LOOKUP_FHEDT_WEIGHTS
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt_weight/0016_oklab_default || {
    echo "not ok 1 - FHEDT oklab default"
    exit 0
}

echo "ok 1 - FHEDT oklab default"
exit 0
