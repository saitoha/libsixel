#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Verify ciede2000 reference.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "assessment/0001_ciede2000_reference" || {
    echo "not ok 1 - ciede2000 reference"
    exit 0
}
echo "ok 1 - ciede2000 reference"
exit 0
