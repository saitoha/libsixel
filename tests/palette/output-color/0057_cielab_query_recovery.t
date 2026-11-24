#!/bin/sh
# Verify standalone cielab query recovery.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/direct_float" "58" || {
    printf '\n%s\n' "not ok 1 - standalone cielab query recovery"
    exit 0
}
printf '\n%s\n' "ok 1 - standalone cielab query recovery"
exit 0
