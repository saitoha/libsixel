#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify gray alpha zero numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0172_gray_alpha_zero_numeric 1>&2 || {
    printf '\n%s\n' "not ok 1 - gray alpha zero numeric"
    exit 0
}
printf '\n%s\n' "ok 1 - gray alpha zero numeric"
exit 0
