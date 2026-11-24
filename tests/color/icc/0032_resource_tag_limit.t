#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify the builtin parser resource boundary.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0032_resource_tag_limit || {
    printf '\n%s\n' "not ok 1 - 257 ICC tags hit the resource limit"
    exit 0
}

printf '\n%s\n' "ok 1 - 257 ICC tags hit the resource limit"
exit 0
