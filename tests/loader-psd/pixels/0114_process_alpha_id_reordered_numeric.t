#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify process alpha id reordered numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0138_process_alpha_id_reordered_numeric 1>&2 || {
    echo "not ok 1 - process alpha id reordered numeric"
    exit 0
}
echo "ok 1 - process alpha id reordered numeric"
exit 0
