#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
# Verify ordered gamma oracle large.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/ordered/0016_ord_gamma_oracle_large || {
    echo "not ok 1 - ordered gamma oracle large"
    exit 0
}
echo "ok 1 - ordered gamma oracle large"
exit 0
