#!/bin/sh
# Verify emit runs limited stream bytes.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder-core/0062_emit_runs_limited" || {
    printf '\n%s\n' "not ok 1 - emit runs limited stream bytes"
    exit 0
}

printf '\n%s\n' "ok 1 - emit runs limited stream bytes"
exit 0
