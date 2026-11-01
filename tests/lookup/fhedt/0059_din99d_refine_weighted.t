#!/bin/sh
# Verify FHEDT din99d refine weighted.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

unset SIXEL_LOOKUP_FHEDT_WEIGHTS
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt_weight/0007_din99d_refine_weighted || {
    echo "not ok 1 - FHEDT din99d refine weighted"
    exit 0
}

echo "ok 1 - FHEDT din99d refine weighted"
exit 0
