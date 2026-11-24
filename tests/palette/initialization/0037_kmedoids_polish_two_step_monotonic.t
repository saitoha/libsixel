#!/bin/sh
# TAP test: two-step PAM polish keeps objective monotonic.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" polish-two-step-monotonic \
    >/dev/null || {
    printf '\n%s\n' "not ok 1 - kmedoids two-step polish monotonicity check failed"
    exit 0
}

printf '\n%s\n' "ok 1 - kmedoids two-step polish keeps objective monotonic"
exit 0
