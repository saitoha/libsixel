#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/blockcull.md
# Verify blockcull cielab fraction.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/blockcull/0009_blockcull_cielab_fraction || {
    printf '\n%s\n' "not ok 1 - blockcull cielab fraction"
    exit 0
}
printf '\n%s\n' "ok 1 - blockcull cielab fraction"
exit 0
