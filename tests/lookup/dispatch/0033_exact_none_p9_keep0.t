#!/bin/sh
# Verify exact none p9 keep0.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup none \
    "lookup/dispatch/0033_exact_none_p9_keep0" || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - exact none p9 keep0"
    exit 0
}
printf '\n%s\n' "ok 1 - exact none p9 keep0"
exit 0
