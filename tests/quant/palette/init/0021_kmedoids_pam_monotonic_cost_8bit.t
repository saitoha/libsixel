#!/bin/sh
# TAP test: PAM objective should not increase with additional iterations.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" pam-monotonic-8bit >/dev/null || {
    echo "not ok" 1 - "kmedoids PAM monotonic-cost check failed"
    exit 0
}

echo "ok" 1 - "kmedoids PAM monotonic-cost check passed"
exit 0
