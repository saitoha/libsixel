#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
# Verify ordered gamma oracle.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/ordered/0011_ord_gamma_oracle || {
    echo "not ok 1 - ordered gamma oracle"
    exit 0
}
echo "ok 1 - ordered gamma oracle"
exit 0
