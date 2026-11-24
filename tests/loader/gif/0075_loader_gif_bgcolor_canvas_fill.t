#!/bin/sh

# Policy: docs/loader/background-policy.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0054_loader_gif_bgcolor_canvas_fill" || {
    printf '\n%s\n' "not ok 1 - file_first GIF background canvas fill"
    exit 0
}

printf '\n%s\n' "ok 1 - file_first GIF background canvas fill"
exit 0
