#!/bin/sh
# Policy: docs/loader/builtin/psd.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify alternate savedmask reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0085_alternate_savedmask_reject 1>&2 || {
    echo "not ok 1 - alternate savedmask reject"
    exit 0
}
echo "ok 1 - alternate savedmask reject"
exit 0
