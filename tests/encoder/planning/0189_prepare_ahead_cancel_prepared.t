#!/bin/sh
# Policy: docs/threading/animation.md
# Verify prepare ahead cancel prepared.
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    encoder/planning/0189_prepare_ahead_cancel_prepared >/dev/null || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP required thread or GPU support unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - prepare ahead cancel prepared'
    exit 0
}
echo 'ok 1 - prepare ahead cancel prepared'
exit 0
