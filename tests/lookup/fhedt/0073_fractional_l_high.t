#!/bin/sh
# Verify FHEDT fractional l high.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

unset SIXEL_LOOKUP_FHEDT_WEIGHTS
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt_weight/0021_fractional_l_high || {
    printf '\n%s\n' "not ok 1 - FHEDT fractional l high"
    exit 0
}

printf '\n%s\n' "ok 1 - FHEDT fractional l high"
exit 0
