#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0114_colorspace_legacy_float_pair || {
    echo "not ok 1 - mixed ColorSpace legacy LUTs support RGB float to RGB float"
    exit 0
}

echo "ok 1 - mixed ColorSpace legacy LUTs support RGB float to RGB float"
exit 0
