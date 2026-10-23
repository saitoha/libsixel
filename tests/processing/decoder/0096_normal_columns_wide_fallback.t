#!/bin/sh
# Policy: docs/decoder/pipeline/README.md
# Verify normal columns wide fallback.
set -eux
echo "1..1"
set -v

SIXEL_THREADING_POLICY=1 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0096_normal_columns_wide_fallback" || {
    echo "not ok 1 - normal_columns_wide_fallback"
    exit 0
}
echo "ok 1 - normal_columns_wide_fallback"
exit 0
