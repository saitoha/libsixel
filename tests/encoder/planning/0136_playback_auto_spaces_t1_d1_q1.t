#!/bin/sh
# playback auto spaces t1 d1 q1.
# Policy: docs/encoder/palette-application/working-colorspace/README.md
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread support disabled"
    exit 0
}

echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0096_playback_auto_colorspaces 1 1 1 0 || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP thread support disabled"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - playback auto spaces t1 d1 q1"
    exit 0
}
echo "ok 1 - playback auto spaces t1 d1 q1"
exit 0
