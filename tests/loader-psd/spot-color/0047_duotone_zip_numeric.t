#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify duotone zip numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0003_duotone_zip_numeric 1>&2 || {
    printf '\n%s\n' "not ok 1 - duotone zip numeric"
    exit 0
}
printf '\n%s\n' "ok 1 - duotone zip numeric"
exit 0
