#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
# Verify ordered oklab oracle large.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/ordered/0018_ord_oklab_oracle_large || {
    echo "not ok 1 - ordered oklab oracle large"
    exit 0
}
echo "ok 1 - ordered oklab oracle large"
exit 0
