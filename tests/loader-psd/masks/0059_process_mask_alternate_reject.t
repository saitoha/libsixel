#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process mask alternate reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0112_process_mask_alternate_reject 1>&2 || {
    echo "not ok 1 - process mask alternate reject"
    exit 0
}
echo "ok 1 - process mask alternate reject"
exit 0
