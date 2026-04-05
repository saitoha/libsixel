#!/bin/sh
# TAP test: one-step PAM polish does not increase objective cost.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" pam-polish-monotonic >/dev/null || {
    echo "not ok" 1 - "kmedoids PAM polish monotonicity check failed"
    exit 0
}

echo "ok" 1 - "kmedoids PAM polish keeps objective monotonic"
exit 0
