#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Reserved UI appears and disappears with cpu latest t0
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0112_reserved_delta_cpu_latest_t0 || {
    echo "not ok 1 - Reserved UI appears and disappears with cpu latest t0"
    exit 0
}
echo "ok 1 - Reserved UI appears and disappears with cpu latest t0"
exit 0
