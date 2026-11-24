#!/bin/sh
# Policy: docs/encoder/output/or-mode/README.md
# Lock OR plane selectors at this palette boundary.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder-core/0007_ormode_one_color_plane" || {
    printf '\n%s\n' "not ok 1 - ormode_one_color_plane"
    exit 0
}

printf '\n%s\n' "ok 1 - ormode_one_color_plane"
exit 0
