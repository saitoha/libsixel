#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process alpha omitted ids numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0135_process_alpha_omitted_ids_numeric 1>&2 || {
    echo "not ok 1 - process alpha omitted ids numeric"
    exit 0
}
echo "ok 1 - process alpha omitted ids numeric"
exit 0
