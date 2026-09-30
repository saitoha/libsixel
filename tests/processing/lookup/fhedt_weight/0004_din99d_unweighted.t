#!/bin/sh
# Verify FHEDT din99d unweighted.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

unset SIXEL_LOOKUP_FHEDT_WEIGHTS
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt_weight/0004_din99d_unweighted || {
    echo "not ok 1 - FHEDT din99d unweighted"
    exit 0
}

echo "ok 1 - FHEDT din99d unweighted"
exit 0
