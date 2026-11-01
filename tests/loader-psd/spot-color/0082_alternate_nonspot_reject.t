#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify alternate nonspot reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0065_alternate_nonspot_reject 1>&2 || {
    echo "not ok 1 - alternate nonspot reject"
    exit 0
}
echo "ok 1 - alternate nonspot reject"
exit 0
