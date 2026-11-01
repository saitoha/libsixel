#!/bin/sh
# Policy: docs/threading/encoder.md
set -eux
test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gamut/0065_pipeline_overlap || {
    echo "not ok 1 - pipeline overlap and budgets"
    exit 0
}
echo "ok 1 - pipeline overlap and budgets"
exit 0
