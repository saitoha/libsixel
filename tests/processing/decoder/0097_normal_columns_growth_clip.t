#!/bin/sh
# Policy: docs/decoder/pipeline/README.md
# Verify normal columns growth clip.
set -eux
echo "1..1"
set -v

SIXEL_THREADS=1 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0097_normal_columns_growth_clip" || {
    echo "not ok 1 - normal_columns_growth_clip"
    exit 0
}
echo "ok 1 - normal_columns_growth_clip"
exit 0
