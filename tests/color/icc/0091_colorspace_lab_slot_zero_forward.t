#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0091_colorspace_lab_slot_zero_forward || {
    printf '\n%s\n' "not ok 1 - ColorSpace Lab uses legacy slot zero forward when slot one is absent"
    exit 0
}

printf '\n%s\n' "ok 1 - ColorSpace Lab uses legacy slot zero forward when slot one is absent"
exit 0
