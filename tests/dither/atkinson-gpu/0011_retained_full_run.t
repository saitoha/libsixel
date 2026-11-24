#!/bin/sh
# Verify atkinson device retained full run.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "dither/atkinson-gpu/0011_retained_full_run" || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - atkinson device retained full run"
    exit 0
}
printf '\n%s\n' "ok 1 - atkinson device retained full run"
exit 0
