#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify cmyk alpha background numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0221_cmyk_alpha_background_numeric 1>&2 || {
    echo "not ok 1 - cmyk alpha background numeric"
    exit 0
}
echo "ok 1 - cmyk alpha background numeric"
exit 0
