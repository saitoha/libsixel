#!/bin/sh
# Retained commit execution: workers four rgb.
# Policy: docs/threading/encoder.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder/indexed/0106_retained_workers_four_rgb" || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' 'ok 1 # SKIP required thread support is unavailable'
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - retained workers four rgb'
    exit 0
}
printf '\n%s\n' 'ok 1 - retained workers four rgb'
exit 0
