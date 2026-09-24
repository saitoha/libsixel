#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify cmyk alpha psb zip numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0225_cmyk_alpha_psb_zip_numeric 1>&2 || {
    echo "not ok 1 - cmyk alpha psb zip numeric"
    exit 0
}
echo "ok 1 - cmyk alpha psb zip numeric"
exit 0
