#!/bin/sh
# Verify FHEDT gamma channel ratio.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

unset SIXEL_LOOKUP_FHEDT_WEIGHTS
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt_weight/0026_gamma_channel_ratio || {
    printf '\n%s\n' "not ok 1 - FHEDT gamma channel ratio"
    exit 0
}

printf '\n%s\n' "ok 1 - FHEDT gamma channel ratio"
exit 0
