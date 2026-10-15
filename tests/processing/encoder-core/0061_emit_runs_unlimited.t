#!/bin/sh
# Verify emit runs unlimited stream bytes.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder-core/0061_emit_runs_unlimited" || {
    echo "not ok 1 - emit runs unlimited stream bytes"
    exit 0
}

echo "ok 1 - emit runs unlimited stream bytes"
exit 0
