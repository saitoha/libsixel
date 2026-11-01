#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Reserved UI appears and disappears with cpu latest t48
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0113_reserved_delta_cpu_latest_t48 || {
    echo "not ok 1 - Reserved UI appears and disappears with cpu latest t48"
    exit 0
}
echo "ok 1 - Reserved UI appears and disappears with cpu latest t48"
exit 0
