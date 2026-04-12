#!/bin/sh
# Verify GIF background policy default/invalid fallback and explicit-first switch.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/0054_loader_gif_bgcolor_canvas_fill" || {
    echo "not ok 1 - loader/0054_loader_gif_bgcolor_canvas_fill (default policy)"
    exit 0
}

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_BACKGROUND_POLICY=invalid" \
    "loader/0054_loader_gif_bgcolor_canvas_fill" || {
    echo "not ok 1 - loader/0054_loader_gif_bgcolor_canvas_fill (invalid fallback)"
    exit 0
}

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_BACKGROUND_POLICY=explicit_first" \
    "loader/0054_loader_gif_bgcolor_canvas_fill" || {
    echo "not ok 1 - loader/0054_loader_gif_bgcolor_canvas_fill (explicit_first)"
    exit 0
}

echo "ok 1 - GIF background policy default/fallback/switch verified"
exit 0
