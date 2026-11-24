#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify gray16 alpha icc numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0202_gray16_alpha_icc_numeric 1>&2 || {
    printf '\n%s\n' "not ok 1 - gray16 alpha icc numeric"
    exit 0
}
printf '\n%s\n' "ok 1 - gray16 alpha icc numeric"
exit 0
