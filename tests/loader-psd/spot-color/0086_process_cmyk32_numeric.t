#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process cmyk32 numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0095_process_cmyk32_numeric 1>&2 || {
    echo "not ok 1 - process cmyk32 numeric"
    exit 0
}
echo "ok 1 - process cmyk32 numeric"
exit 0
