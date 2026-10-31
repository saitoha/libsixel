#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Automatic palette scheduling retains fixed colors and samples only video.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "planner/pipeline/0100_reserved_auto_source" || {
    echo "not ok 1 - reserved auto source"
    exit 0
}
echo "ok 1 - reserved auto source"
exit 0
