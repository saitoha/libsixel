#!/bin/sh

# Policy: docs/loader/background-policy.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0070_loader_gif_bgcolor_canvas_fill_explicit_first" || {
    echo "not ok 1 - explicit_first GIF background canvas fill"
    exit 0
}

echo "ok 1 - explicit_first GIF background canvas fill"
exit 0
