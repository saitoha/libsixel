#!/bin/sh
# Verify standalone cielab getter read.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/direct_float" "54" || {
    printf '\n%s\n' "not ok 1 - standalone cielab getter read"
    exit 0
}
printf '\n%s\n' "ok 1 - standalone cielab getter read"
exit 0
