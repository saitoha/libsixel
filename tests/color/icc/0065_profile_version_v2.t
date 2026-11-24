#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0065_profile_version_v2 || {
    printf '\n%s\n' "not ok 1 - an ICC v2.4 LUT profile is accepted"
    exit 0
}

printf '\n%s\n' "ok 1 - an ICC v2.4 LUT profile is accepted"
exit 0
