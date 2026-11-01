#!/bin/sh
# Verify palette color growth.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0004_palette_color_growth" || {
    echo "not ok 1 - palette color growth"
    exit 0
}
echo "ok 1 - palette color growth"
exit 0
