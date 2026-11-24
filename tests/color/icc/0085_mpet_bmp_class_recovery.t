#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0085_mpet_bmp_class_recovery || {
    printf '\n%s\n' "not ok 1 - builtin BMP rejects DeviceLink and can load a valid profile next"
    exit 0
}

printf '\n%s\n' "ok 1 - builtin BMP rejects DeviceLink and can load a valid profile next"
exit 0
