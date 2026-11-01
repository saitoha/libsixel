#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process identifier duplicate reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0124_process_identifier_duplicate_reject 1>&2 || {
    echo "not ok 1 - process identifier duplicate reject"
    exit 0
}
echo "ok 1 - process identifier duplicate reject"
exit 0
