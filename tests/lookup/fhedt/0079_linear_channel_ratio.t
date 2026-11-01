#!/bin/sh
# Verify FHEDT linear channel ratio.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

unset SIXEL_LOOKUP_FHEDT_WEIGHTS
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt_weight/0027_linear_channel_ratio || {
    echo "not ok 1 - FHEDT linear channel ratio"
    exit 0
}

echo "ok 1 - FHEDT linear channel ratio"
exit 0
