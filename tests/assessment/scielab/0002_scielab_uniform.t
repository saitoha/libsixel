#!/bin/sh
# Policy: docs/quality/metrics.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "assessment/0027_scielab_uniform" || {
    printf '\n%s\n' "not ok 1 - scielab uniform"
    exit 0
}
printf '\n%s\n' "ok 1 - scielab uniform"
exit 0
