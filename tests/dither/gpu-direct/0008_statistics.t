#!/bin/sh
# Verify the public GPU statistics advance after a direct GPU apply.
set -eux

echo "1..1"
set -v

# Keep capability initialization and real dispatch in one timed process.
status=0

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette-direct gpu-direct/0008_statistics || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP direct GPU palette apply is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - GPU statistics advance after a direct GPU apply"
    exit 0
}
echo "ok 1 - GPU statistics advance after a direct GPU apply"
exit 0
