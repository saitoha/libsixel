#!/bin/sh
# Retained commit execution: workers two rgb.
# Policy: docs/threading/encoder.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder/indexed/0105_retained_workers_two_rgb" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP required thread support is unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - retained workers two rgb'
    exit 0
}
echo 'ok 1 - retained workers two rgb'
exit 0
