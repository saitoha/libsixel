#!/bin/sh
# Verify GPU partial rectangle pixel count.
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta/gpu-rectangle/0017_pixel_count || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - GPU partial rectangle pixel count"
    exit 0
}
echo "ok 1 - GPU partial rectangle pixel count"
exit 0
