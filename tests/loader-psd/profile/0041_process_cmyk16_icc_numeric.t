#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process cmyk16 icc numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0098_process_cmyk16_icc_numeric 1>&2 || {
    echo "not ok 1 - process cmyk16 icc numeric"
    exit 0
}
echo "ok 1 - process cmyk16 icc numeric"
exit 0
