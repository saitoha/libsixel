#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify cmyk16 alpha icc numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0218_cmyk16_alpha_icc_numeric 1>&2 || {
    echo "not ok 1 - cmyk16 alpha icc numeric"
    exit 0
}
echo "ok 1 - cmyk16 alpha icc numeric"
exit 0
