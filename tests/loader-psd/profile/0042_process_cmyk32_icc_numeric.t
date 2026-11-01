#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process cmyk32 icc numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0099_process_cmyk32_icc_numeric 1>&2 || {
    echo "not ok 1 - process cmyk32 icc numeric"
    exit 0
}
echo "ok 1 - process cmyk32 icc numeric"
exit 0
