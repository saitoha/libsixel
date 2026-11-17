#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify that profile allocation is reported separately.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0030_status_profile_allocation || {
    echo "not ok 1 - profile allocation is reported separately"
    exit 0
}

echo "ok 1 - profile allocation is reported separately"
exit 0
