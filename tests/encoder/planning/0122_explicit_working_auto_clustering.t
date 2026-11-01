#!/bin/sh
# Policy: docs/encoder/palette-application/working-colorspace/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/0007_explicit_working_auto_clustering 1>&2 || {
    echo "not ok 1 - explicit working auto clustering"
    exit 0
}
echo "ok 1 - explicit working auto clustering"
exit 0
