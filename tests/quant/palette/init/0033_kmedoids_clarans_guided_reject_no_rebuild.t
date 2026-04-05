#!/bin/sh
# TAP test: CLARANS guided state skips reject-time full rebuilds.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" clarans-guided-reject-no-rebuild \
    >/dev/null || {
    echo "not ok" 1 - "kmedoids CLARANS reject path rebuild suppression failed"
    exit 0
}

echo "ok" 1 - "kmedoids CLARANS reject path avoids extra full rebuild"
exit 0
