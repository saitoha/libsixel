#!/bin/sh
# Verify eytzinger cielab metric.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/eytzinger/0011_eyt_cielab_metric || {
    echo "not ok 1 - eytzinger cielab metric"
    exit 0
}

echo "ok 1 - eytzinger cielab metric"
exit 0
