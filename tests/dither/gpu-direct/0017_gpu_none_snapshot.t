#!/bin/sh
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" positional_gpu_none_snapshot || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP GPU device unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - GPU none snapshot"
    exit 0
}
printf '\n%s\n' "ok 1 - GPU none snapshot"
exit 0
