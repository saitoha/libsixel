#!/bin/sh
# Verify palette map gpu.
# Policy: docs/decoder/pipeline/README.md

set -eux

echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0051_palette_map_gpu" || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 - 0051_palette_map_gpu # SKIP GPU dequant unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - 0051_palette_map_gpu"
    exit 0
}

printf '\n%s\n' "ok 1 - 0051_palette_map_gpu"
exit 0
