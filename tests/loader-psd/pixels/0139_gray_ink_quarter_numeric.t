#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify gray ink quarter numeric at the public loader boundary.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0265_gray_ink_quarter_numeric 1>&2 || {
    printf '\n%s\n' "not ok 1 - gray ink quarter numeric"
    exit 0
}
printf '\n%s\n' "ok 1 - gray ink quarter numeric"
exit 0
