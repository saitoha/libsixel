#!/bin/sh
# Verify p3 float roundtrip.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cms/0010_p3_float_roundtrip" || {
    echo "not ok 1 - p3 float roundtrip"
    exit 0
}

echo "ok 1 - p3 float roundtrip"
exit 0
