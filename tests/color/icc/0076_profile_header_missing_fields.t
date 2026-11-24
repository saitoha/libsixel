#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0076_profile_header_missing_fields || {
    printf '\n%s\n' "not ok 1 - zero ICC header fields retain builtin compatibility"
    exit 0
}

printf '\n%s\n' "ok 1 - zero ICC header fields retain builtin compatibility"
exit 0
