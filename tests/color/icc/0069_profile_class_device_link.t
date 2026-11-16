#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0069_profile_class_device_link || {
    echo "not ok 1 - a DeviceLink is rejected as a PCS endpoint"
    exit 0
}

echo "ok 1 - a DeviceLink is rejected as a PCS endpoint"
exit 0
