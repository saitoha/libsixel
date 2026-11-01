#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify duotone duplicate reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0039_duotone_duplicate_reject 1>&2 || {
    echo "not ok 1 - duotone duplicate reject"
    exit 0
}
echo "ok 1 - duotone duplicate reject"
exit 0
