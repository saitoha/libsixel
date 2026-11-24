#!/bin/sh
# TAP test: radius-constrained SSE polish never worsens the selected radius.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0003_kcenter_constraints" radius-constrained-sse-polish >/dev/null || {
    printf '\n%s\n' "not ok 1 - kcenter radius-constrained SSE polish check failed"
    exit 0
}

printf '\n%s\n' "ok 1 - kcenter radius-constrained SSE polish check passed"
exit 0
