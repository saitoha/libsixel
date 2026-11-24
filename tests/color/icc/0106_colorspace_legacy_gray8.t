#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0106_colorspace_legacy_gray8 || {
    printf '\n%s\n' "not ok 1 - ColorSpace Gray8 retains its legacy XYZ encoding"
    exit 0
}

printf '\n%s\n' "ok 1 - ColorSpace Gray8 retains its legacy XYZ encoding"
exit 0
