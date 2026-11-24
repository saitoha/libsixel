#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/blockcull.md
# Verify blockcull cielab metric.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/blockcull/0031_blockcull_cielab_metric || {
    printf '\n%s\n' "not ok 1 - blockcull cielab metric"
    exit 0
}
printf '\n%s\n' "ok 1 - blockcull cielab metric"
exit 0
