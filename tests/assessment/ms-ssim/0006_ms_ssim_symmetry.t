#!/bin/sh
# Policy: docs/quality/metrics.md
# Verify MS-SSIM symmetry with independent values.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" assessment/0017_ms_ssim 5 1>&2 || {
    echo "not ok 1 - MS-SSIM symmetry"
    exit 0
}
echo "ok 1 - MS-SSIM symmetry"
exit 0
