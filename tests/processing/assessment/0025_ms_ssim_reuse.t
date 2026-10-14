#!/bin/sh
# Policy: docs/quality/metrics.md
# Verify MS-SSIM reuse with independent values.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" assessment/0017_ms_ssim 8 1>&2 || {
    echo "not ok 1 - MS-SSIM reuse"
    exit 0
}
echo "ok 1 - MS-SSIM reuse"
exit 0
