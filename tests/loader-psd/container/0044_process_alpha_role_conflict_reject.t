#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process alpha role conflict reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0139_process_alpha_role_conflict_reject 1>&2 || {
    echo "not ok 1 - process alpha role conflict reject"
    exit 0
}
echo "ok 1 - process alpha role conflict reject"
exit 0
