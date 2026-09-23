#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify sof3 rgb planar against every original sample.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/jpeg/0184_sof3_rgb_planar_numeric 1>&2 || {
    echo "not ok 1 - sof3_rgb_planar numeric reconstruction"
    exit 0
}
echo "ok 1 - sof3_rgb_planar numeric reconstruction"
exit 0
