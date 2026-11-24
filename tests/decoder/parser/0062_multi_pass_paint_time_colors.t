#!/bin/sh
# Verify paint time colors.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0057_multi_pass_paint_time_colors" || {
    printf '\n%s\n' "not ok 1 - paint time colors"
    exit 0
}

printf '\n%s\n' "ok 1 - paint time colors"
exit 0
