#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0092_colorspace_lab_slot_zero_reverse || {
    echo "not ok 1 - ColorSpace Lab uses legacy slot zero in reverse when slot one is absent"
    exit 0
}

echo "ok 1 - ColorSpace Lab uses legacy slot zero in reverse when slot one is absent"
exit 0
