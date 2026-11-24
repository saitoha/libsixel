#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0114_colorspace_legacy_float_pair || {
    printf '\n%s\n' "not ok 1 - mixed ColorSpace legacy LUTs support RGB float to RGB float"
    exit 0
}

printf '\n%s\n' "ok 1 - mixed ColorSpace legacy LUTs support RGB float to RGB float"
exit 0
