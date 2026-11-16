#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0070_profile_class_abstract || {
    echo "not ok 1 - an Abstract profile is rejected as a PCS endpoint"
    exit 0
}

echo "ok 1 - an Abstract profile is rejected as a PCS endpoint"
exit 0
