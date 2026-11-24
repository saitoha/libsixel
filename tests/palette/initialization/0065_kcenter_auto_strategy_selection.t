#!/bin/sh
# TAP test: auto strategy follows quality and point-count branch rules.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0003_kcenter_constraints" auto-strategy-selection >/dev/null || {
    printf '\n%s\n' "not ok 1 - kcenter auto strategy selection check failed"
    exit 0
}

printf '\n%s\n' "ok 1 - kcenter auto strategy selection check passed"
exit 0
