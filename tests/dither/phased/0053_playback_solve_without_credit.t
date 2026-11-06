#!/bin/sh
# Policy: docs/encoder/palette-application/dithering/phased.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" phased/playback_solve_without_credit || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP required backend unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - playback solve without credit'
    exit 0
}
echo 'ok 1 - playback solve without credit'
exit 0
