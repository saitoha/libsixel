#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify duotone 32 numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0007_duotone_32_numeric 1>&2 || {
    printf '\n%s\n' "not ok 1 - duotone 32 numeric"
    exit 0
}
printf '\n%s\n' "ok 1 - duotone 32 numeric"
exit 0
