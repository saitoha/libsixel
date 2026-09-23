#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify alternate version reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0056_alternate_version_reject 1>&2 || {
    echo "not ok 1 - alternate version reject"
    exit 0
}
echo "ok 1 - alternate version reject"
exit 0
