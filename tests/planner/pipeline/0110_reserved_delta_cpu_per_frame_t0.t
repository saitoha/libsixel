#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Reserved UI appears and disappears with cpu per frame t0
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0110_reserved_delta_cpu_per_frame_t0 || {
    echo "not ok 1 - Reserved UI appears and disappears with cpu per frame t0"
    exit 0
}
echo "ok 1 - Reserved UI appears and disappears with cpu per frame t0"
exit 0
