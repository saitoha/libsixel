#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process alpha mt16 reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0108_process_alpha_mt16_reject 1>&2 || {
    echo "not ok 1 - process alpha mt16 reject"
    exit 0
}
echo "ok 1 - process alpha mt16 reject"
exit 0
