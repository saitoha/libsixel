#!/bin/sh
# Retained commit execution: default serial.
# Policy: docs/threading/encoder.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder/indexed/0114_retained_default_serial" || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' 'ok 1 # SKIP required thread support is unavailable'
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - retained default serial'
    exit 0
}
printf '\n%s\n' 'ok 1 - retained default serial'
exit 0
