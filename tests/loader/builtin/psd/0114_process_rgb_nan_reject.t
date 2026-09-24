#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process rgb nan reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0114_process_rgb_nan_reject 1>&2 || {
    echo "not ok 1 - process rgb nan reject"
    exit 0
}
echo "ok 1 - process rgb nan reject"
exit 0
