#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify lab alpha display omitted numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0244_lab_alpha_display_omitted_numeric 1>&2 || {
    echo "not ok 1 - lab alpha display omitted numeric"
    exit 0
}
echo "ok 1 - lab alpha display omitted numeric"
exit 0
