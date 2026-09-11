#!/bin/sh
# Policy: docs/functionality/or-mode.md
# Lock OR plane selectors at this palette boundary.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder-core/0008_ormode_two_color_plane" || {
    echo "not ok 1 - ormode_two_color_plane"
    exit 0
}

echo "ok 1 - ormode_two_color_plane"
exit 0
