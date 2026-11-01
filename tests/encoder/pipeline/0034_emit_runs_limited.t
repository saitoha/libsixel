#!/bin/sh
# Verify emit runs limited stream bytes.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder-core/0062_emit_runs_limited" || {
    echo "not ok 1 - emit runs limited stream bytes"
    exit 0
}

echo "ok 1 - emit runs limited stream bytes"
exit 0
