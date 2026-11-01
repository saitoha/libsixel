#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/gridlut.md
# gridlut completion failure falls back and rebuilds
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette \
    gpu-palette/0029_gridlut_completion_failure || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - gridlut completion failure falls back and rebuilds"
    exit 0
}
echo "ok 1 - gridlut completion failure falls back and rebuilds"
exit 0
