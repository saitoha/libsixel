#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify cmyk alpha native8 numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0235_cmyk_alpha_native8_numeric 1>&2 || {
    printf '\n%s\n' "not ok 1 - cmyk alpha native8 numeric"
    exit 0
}
printf '\n%s\n' "ok 1 - cmyk alpha native8 numeric"
exit 0
