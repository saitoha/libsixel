#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify cmyk8 alpha icc numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0217_cmyk8_alpha_icc_numeric 1>&2 || {
    echo "not ok 1 - cmyk8 alpha icc numeric"
    exit 0
}
echo "ok 1 - cmyk8 alpha icc numeric"
exit 0
