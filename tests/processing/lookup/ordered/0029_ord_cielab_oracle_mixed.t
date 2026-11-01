#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
# Verify ordered cielab oracle mixed.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/ordered/0029_ord_cielab_oracle_mixed || {
    echo "not ok 1 - ordered cielab oracle mixed"
    exit 0
}
echo "ok 1 - ordered cielab oracle mixed"
exit 0
