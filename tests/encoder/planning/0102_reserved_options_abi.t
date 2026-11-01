#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Old playback options neither overwrite nor read the absent source field.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "planner/pipeline/0102_reserved_options_abi" || {
    echo "not ok 1 - reserved options abi"
    exit 0
}
echo "ok 1 - reserved options abi"
exit 0
