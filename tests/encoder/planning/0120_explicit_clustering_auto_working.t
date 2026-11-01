#!/bin/sh
# Verify explicit clustering auto working at planner and encoder boundaries.
# Policy: docs/encoder/palette-application/working-colorspace/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/0005_explicit_clustering_auto_working 1>&2 || {
    echo "not ok 1 - explicit clustering auto working"
    exit 0
}
echo "ok 1 - explicit clustering auto working"
exit 0
