#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0115_colorspace_legacy_byte_float_pair || {
    echo "not ok 1 - mixed ColorSpace legacy LUTs support RGB bytes to RGB float"
    exit 0
}

echo "ok 1 - mixed ColorSpace legacy LUTs support RGB bytes to RGB float"
exit 0
