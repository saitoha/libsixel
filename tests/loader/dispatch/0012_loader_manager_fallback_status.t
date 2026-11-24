#!/bin/sh
# Policy: docs/loader/README.md
# Verify the manager distinguishes fallback and terminal statuses.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0126_loader_manager_fallback_status" || {
    printf '\n%s\n' "not ok 1 - loader manager fallback status partition"
    exit 0
}

printf '\n%s\n' "ok 1 - loader manager fallback status partition is exact"
exit 0
