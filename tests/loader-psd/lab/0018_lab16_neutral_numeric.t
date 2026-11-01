#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify lab16 neutral numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0186_lab16_neutral_numeric 1>&2 || {
    echo "not ok 1 - lab16 neutral numeric"
    exit 0
}
echo "ok 1 - lab16 neutral numeric"
exit 0
