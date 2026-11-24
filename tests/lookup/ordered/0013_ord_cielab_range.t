#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
# Verify ordered cielab range.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/ordered/0004_ord_cielab_range || {
    printf '\n%s\n' "not ok 1 - ordered cielab range"
    exit 0
}
printf '\n%s\n' "ok 1 - ordered cielab range"
exit 0
