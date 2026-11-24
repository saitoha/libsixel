#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
# Verify ordered cielab oracle small.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/ordered/0024_ord_cielab_oracle_small || {
    printf '\n%s\n' "not ok 1 - ordered cielab oracle small"
    exit 0
}
printf '\n%s\n' "ok 1 - ordered cielab oracle small"
exit 0
