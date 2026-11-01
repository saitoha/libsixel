#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
# Verify ordered cielab oracle.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/ordered/0014_ord_cielab_oracle || {
    echo "not ok 1 - ordered cielab oracle"
    exit 0
}
echo "ok 1 - ordered cielab oracle"
exit 0
