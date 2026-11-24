#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify the builtin parser resource boundary.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0034_resource_alloc_recovery || {
    printf '\n%s\n' "not ok 1 - ICC allocation excess leaves a clean retry"
    exit 0
}

printf '\n%s\n' "ok 1 - ICC allocation excess leaves a clean retry"
exit 0
