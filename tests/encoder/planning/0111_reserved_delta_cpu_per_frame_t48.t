#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Reserved UI appears and disappears with cpu per frame t48
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0111_reserved_delta_cpu_per_frame_t48 || {
    printf '\n%s\n' "not ok 1 - Reserved UI appears and disappears with cpu per frame t48"
    exit 0
}
printf '\n%s\n' "ok 1 - Reserved UI appears and disappears with cpu per frame t48"
exit 0
