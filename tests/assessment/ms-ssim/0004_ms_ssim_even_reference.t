#!/bin/sh
# Policy: docs/quality/metrics.md
# Verify MS-SSIM even reference with independent values.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" assessment/0017_ms_ssim 3 1>&2 || {
    printf '\n%s\n' "not ok 1 - MS-SSIM even reference"
    exit 0
}
printf '\n%s\n' "ok 1 - MS-SSIM even reference"
exit 0
