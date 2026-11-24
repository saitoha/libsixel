#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process psb16 rle numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0120_process_psb16_rle_numeric 1>&2 || {
    printf '\n%s\n' "not ok 1 - process psb16 rle numeric"
    exit 0
}
printf '\n%s\n' "ok 1 - process psb16 rle numeric"
exit 0
