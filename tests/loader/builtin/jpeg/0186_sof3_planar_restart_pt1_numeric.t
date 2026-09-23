#!/bin/sh
# Policy: docs/loader/builtin/jpeg.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify sof3 planar restart pt1 against every original sample.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/jpeg/0186_sof3_planar_restart_pt1_numeric 1>&2 || {
    echo "not ok 1 - sof3_planar_restart_pt1 numeric reconstruction"
    exit 0
}
echo "ok 1 - sof3_planar_restart_pt1 numeric reconstruction"
exit 0
