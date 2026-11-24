#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Reserved RGB8 colors and an independent gray source survive encoding.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "planner/pipeline/0097_reserved_rgb8_source" || {
    printf '\n%s\n' "not ok 1 - reserved rgb8 source"
    exit 0
}
printf '\n%s\n' "ok 1 - reserved rgb8 source"
exit 0
