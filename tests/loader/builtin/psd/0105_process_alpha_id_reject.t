#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process alpha id reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0105_process_alpha_id_reject 1>&2 || {
    echo "not ok 1 - process alpha id reject"
    exit 0
}
echo "ok 1 - process alpha id reject"
exit 0
