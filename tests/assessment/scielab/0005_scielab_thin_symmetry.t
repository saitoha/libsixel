#!/bin/sh
# Policy: docs/quality/metrics.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "assessment/0030_scielab_thin_symmetry" || {
    printf '\n%s\n' "not ok 1 - scielab thin symmetry"
    exit 0
}
printf '\n%s\n' "ok 1 - scielab thin symmetry"
exit 0
