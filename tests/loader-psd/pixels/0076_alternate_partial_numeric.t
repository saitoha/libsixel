#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify alternate partial numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0048_alternate_partial_numeric 1>&2 || {
    echo "not ok 1 - alternate partial numeric"
    exit 0
}
echo "ok 1 - alternate partial numeric"
exit 0
