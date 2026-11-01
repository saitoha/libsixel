#!/bin/sh
# Verify FHEDT expanded unweighted.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

unset SIXEL_LOOKUP_FHEDT_WEIGHTS
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt_weight/0012_expanded_unweighted || {
    echo "not ok 1 - FHEDT expanded unweighted"
    exit 0
}

echo "ok 1 - FHEDT expanded unweighted"
exit 0
