#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
# Verify ordered gamma range.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/ordered/0001_ord_gamma_range || {
    echo "not ok 1 - ordered gamma range"
    exit 0
}
echo "ok 1 - ordered gamma range"
exit 0
