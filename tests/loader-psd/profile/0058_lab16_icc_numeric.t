#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify lab16 icc numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0180_lab16_icc_numeric 1>&2 || {
    printf '\n%s\n' "not ok 1 - lab16 icc numeric"
    exit 0
}
printf '\n%s\n' "ok 1 - lab16 icc numeric"
exit 0
