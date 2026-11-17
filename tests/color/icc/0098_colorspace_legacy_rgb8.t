#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0098_colorspace_legacy_rgb8 || {
    echo "not ok 1 - ColorSpace legacy RGB8 preserves its XYZ encoding"
    exit 0
}

echo "ok 1 - ColorSpace legacy RGB8 preserves its XYZ encoding"
exit 0
