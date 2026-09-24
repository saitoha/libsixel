#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process tag truncated reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0122_process_tag_truncated_reject 1>&2 || {
    echo "not ok 1 - process tag truncated reject"
    exit 0
}
echo "ok 1 - process tag truncated reject"
exit 0
