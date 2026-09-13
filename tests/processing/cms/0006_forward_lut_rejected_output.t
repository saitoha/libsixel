#!/bin/sh
# Verify forward lut rejected output.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cms/0006_forward_lut_rejected_output" || {
    echo "not ok 1 - forward lut rejected output"
    exit 0
}

echo "ok 1 - forward lut rejected output"
exit 0
