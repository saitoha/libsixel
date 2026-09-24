#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process alpha unmatte gamut numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0150_process_alpha_unmatte_gamut_numeric 1>&2 || {
    echo "not ok 1 - process alpha unmatte gamut numeric"
    exit 0
}
echo "ok 1 - process alpha unmatte gamut numeric"
exit 0
