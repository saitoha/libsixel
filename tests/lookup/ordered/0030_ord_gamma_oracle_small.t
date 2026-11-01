#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
# Verify ordered gamma oracle small.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/ordered/0021_ord_gamma_oracle_small || {
    echo "not ok 1 - ordered gamma oracle small"
    exit 0
}
echo "ok 1 - ordered gamma oracle small"
exit 0
