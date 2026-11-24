#!/bin/sh
# Verify FHEDT common scale large.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

unset SIXEL_LOOKUP_FHEDT_WEIGHTS
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt_weight/0024_common_scale_large || {
    printf '\n%s\n' "not ok 1 - FHEDT common scale large"
    exit 0
}

printf '\n%s\n' "ok 1 - FHEDT common scale large"
exit 0
