#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify that truncated supported payload is malformed.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0026_status_malformed_profile || {
    printf '\n%s\n' "not ok 1 - truncated supported payload is malformed"
    exit 0
}

printf '\n%s\n' "ok 1 - truncated supported payload is malformed"
exit 0
