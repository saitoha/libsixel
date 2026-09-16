#!/bin/sh
# TAP test: radius-constrained SSE polish never worsens the selected radius.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0003_kcenter_constraints" radius-constrained-sse-polish >/dev/null || {
    echo "not ok" 1 - "kcenter radius-constrained SSE polish check failed"
    exit 0
}

echo "ok" 1 - "kcenter radius-constrained SSE polish check passed"
exit 0
