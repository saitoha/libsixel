#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify cmyk32 alpha numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0216_cmyk32_alpha_numeric 1>&2 || {
    echo "not ok 1 - cmyk32 alpha numeric"
    exit 0
}
echo "ok 1 - cmyk32 alpha numeric"
exit 0
