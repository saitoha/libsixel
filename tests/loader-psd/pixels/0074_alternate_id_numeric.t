#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify alternate id numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0046_alternate_id_numeric 1>&2 || {
    echo "not ok 1 - alternate id numeric"
    exit 0
}
echo "ok 1 - alternate id numeric"
exit 0
