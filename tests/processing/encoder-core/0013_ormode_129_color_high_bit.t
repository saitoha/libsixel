#!/bin/sh
# Policy: docs/functionality/or-mode.md
# Lock OR plane selectors at this palette boundary.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder-core/0013_ormode_129_color_high_bit" || {
    echo "not ok 1 - ormode_129_color_high_bit"
    exit 0
}

echo "ok 1 - ormode_129_color_high_bit"
exit 0
