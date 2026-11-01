#!/bin/sh
# TAP test: profile=legacy should match baseline center behavior.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0003_kcenter_constraints" profile-legacy-compatibility >/dev/null || {
    echo "not ok" 1 - "kcenter profile=legacy compatibility check failed"
    exit 0
}

echo "ok" 1 - "kcenter profile=legacy compatibility check passed"
exit 0
