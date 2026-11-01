#!/bin/sh
# Verify FHEDT oklab refine.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

unset SIXEL_LOOKUP_FHEDT_WEIGHTS
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt_weight/0019_oklab_refine || {
    echo "not ok 1 - FHEDT oklab refine"
    exit 0
}

echo "ok 1 - FHEDT oklab refine"
exit 0
