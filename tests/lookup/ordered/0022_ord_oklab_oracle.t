#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
# Verify ordered oklab oracle.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/ordered/0013_ord_oklab_oracle || {
    echo "not ok 1 - ordered oklab oracle"
    exit 0
}
echo "ok 1 - ordered oklab oracle"
exit 0
