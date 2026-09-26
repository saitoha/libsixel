#!/bin/sh
# Accumulated temporal correction reaches the limiter and stores its bounded residual.
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0039_temporal_history" || {
    echo "not ok 1 - temporal_history"
    exit 0
}
echo "ok 1 - temporal_history"
exit 0
