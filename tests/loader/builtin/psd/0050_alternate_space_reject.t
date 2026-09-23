#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify alternate space reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0050_alternate_space_reject 1>&2 || {
    echo "not ok 1 - alternate space reject"
    exit 0
}
echo "ok 1 - alternate space reject"
exit 0
