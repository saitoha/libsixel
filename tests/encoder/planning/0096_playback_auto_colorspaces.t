#!/bin/sh
# Verify mixed AUTO colorspaces match direct frame encoding.
# Policy: docs/encoder/palette-application/working-colorspace/README.md
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "planner/pipeline/0096_playback_auto_colorspaces"
result=$?
set -e
test "$result" -ne 77 || {
    echo "ok 1 - mixed AUTO colorspaces match direct frame encoding # SKIP thread support disabled"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - mixed AUTO colorspaces match direct frame encoding"
    exit 0
}
echo "ok 1 - mixed AUTO colorspaces match direct frame encoding"
exit 0
