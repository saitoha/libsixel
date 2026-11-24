#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0073_profile_class_colorspace_lut || {
    printf '\n%s\n' "not ok 1 - a ColorSpace profile with a LUT is accepted"
    exit 0
}

printf '\n%s\n' "ok 1 - a ColorSpace profile with a LUT is accepted"
exit 0
