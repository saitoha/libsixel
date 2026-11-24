#!/bin/sh
# TAP test: k-center seed reproducibility for hybrid.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0003_kcenter_constraints" seed-hybrid >/dev/null || {
    printf '\n%s\n' "not ok 1 - kcenter seed reproducibility failed for hybrid"
    exit 0
}

printf '\n%s\n' "ok 1 - kcenter seed reproducibility passed for hybrid"
exit 0
