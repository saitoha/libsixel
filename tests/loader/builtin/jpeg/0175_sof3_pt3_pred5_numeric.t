#!/bin/sh
# Policy: docs/loader/builtin/jpeg.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify sof3 pt3 pred5 against every original sample.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/jpeg/0175_sof3_pt3_pred5_numeric 1>&2 || {
    echo "not ok 1 - sof3_pt3_pred5 numeric reconstruction"
    exit 0
}
echo "ok 1 - sof3_pt3_pred5 numeric reconstruction"
exit 0
