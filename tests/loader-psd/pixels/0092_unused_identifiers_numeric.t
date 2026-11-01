#!/bin/sh
# Policy: docs/loader/builtin/psd.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify unused identifiers numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0087_unused_identifiers_numeric 1>&2 || {
    echo "not ok 1 - unused identifiers numeric"
    exit 0
}
echo "ok 1 - unused identifiers numeric"
exit 0
