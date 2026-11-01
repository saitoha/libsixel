#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
# Verify ordered cielab metric.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/ordered/0031_ord_cielab_metric || {
    echo "not ok 1 - ordered cielab metric"
    exit 0
}
echo "ok 1 - ordered cielab metric"
exit 0
