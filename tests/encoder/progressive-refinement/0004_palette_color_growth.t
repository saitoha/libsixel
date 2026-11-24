#!/bin/sh
# Verify palette color growth.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0004_palette_color_growth" || {
    printf '\n%s\n' "not ok 1 - palette color growth"
    exit 0
}
printf '\n%s\n' "ok 1 - palette color growth"
exit 0
