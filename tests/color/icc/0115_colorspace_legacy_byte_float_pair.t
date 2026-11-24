#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0115_colorspace_legacy_byte_float_pair || {
    printf '\n%s\n' "not ok 1 - mixed ColorSpace legacy LUTs support RGB bytes to RGB float"
    exit 0
}

printf '\n%s\n' "ok 1 - mixed ColorSpace legacy LUTs support RGB bytes to RGB float"
exit 0
