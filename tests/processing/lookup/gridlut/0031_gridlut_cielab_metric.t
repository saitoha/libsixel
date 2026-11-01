#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/gridlut.md
# Verify gridlut cielab metric.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/gridlut/0031_gridlut_cielab_metric || {
    echo "not ok 1 - gridlut cielab metric"
    exit 0
}
echo "ok 1 - gridlut cielab metric"
exit 0
