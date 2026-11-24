#!/bin/sh
# Verify named xyz.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cms/0001_named_xyz" || {
    printf '\n%s\n' "not ok 1 - named xyz"
    exit 0
}

printf '\n%s\n' "ok 1 - named xyz"
exit 0
