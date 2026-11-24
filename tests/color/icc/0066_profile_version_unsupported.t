#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0066_profile_version_unsupported || {
    printf '\n%s\n' "not ok 1 - an ICC v5 profile is reported unsupported"
    exit 0
}

printf '\n%s\n' "ok 1 - an ICC v5 profile is reported unsupported"
exit 0
