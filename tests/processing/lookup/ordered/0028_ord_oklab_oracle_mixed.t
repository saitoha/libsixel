#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
# Verify ordered oklab oracle mixed.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/ordered/0028_ord_oklab_oracle_mixed || {
    echo "not ok 1 - ordered oklab oracle mixed"
    exit 0
}
echo "ok 1 - ordered oklab oracle mixed"
exit 0
