#!/bin/sh
# Policy: docs/encoder/palette-application/working-colorspace/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/0006_byte_precision_auto 1>&2 || {
    echo "not ok 1 - byte precision auto"
    exit 0
}
echo "ok 1 - byte precision auto"
exit 0
