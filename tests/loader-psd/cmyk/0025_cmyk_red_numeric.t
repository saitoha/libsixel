#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify cmyk red numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0076_cmyk_red_numeric 1>&2 || {
    printf '\n%s\n' "not ok 1 - cmyk red numeric"
    exit 0
}
printf '\n%s\n' "ok 1 - cmyk red numeric"
exit 0
