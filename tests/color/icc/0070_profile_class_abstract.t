#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0070_profile_class_abstract || {
    printf '\n%s\n' "not ok 1 - an Abstract profile is rejected as a PCS endpoint"
    exit 0
}

printf '\n%s\n' "ok 1 - an Abstract profile is rejected as a PCS endpoint"
exit 0
