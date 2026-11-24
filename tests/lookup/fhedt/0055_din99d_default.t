#!/bin/sh
# Verify FHEDT din99d default.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

unset SIXEL_LOOKUP_FHEDT_WEIGHTS
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt_weight/0003_din99d_default || {
    printf '\n%s\n' "not ok 1 - FHEDT din99d default"
    exit 0
}

printf '\n%s\n' "ok 1 - FHEDT din99d default"
exit 0
