#!/bin/sh
# Verify paired lut applies.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cms/0007_paired_lut_applies" || {
    echo "not ok 1 - paired lut applies"
    exit 0
}

echo "ok 1 - paired lut applies"
exit 0
