#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify that unsupported profile has its own status.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0025_status_unsupported_profile || {
    echo "not ok 1 - unsupported profile has its own status"
    exit 0
}

echo "ok 1 - unsupported profile has its own status"
exit 0
