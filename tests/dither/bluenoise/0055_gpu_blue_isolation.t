#!/bin/sh
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" positional_gpu_blue_isolation || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU device unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - GPU blue isolation"
    exit 0
}
echo "ok 1 - GPU blue isolation"
exit 0
