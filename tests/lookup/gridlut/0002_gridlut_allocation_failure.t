#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/gridlut.md
# gridlut allocation failure falls back and rebuilds
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup gridlut \
    gpu-palette/0027_gridlut_allocation_failure || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP GPU gridlut program unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - gridlut allocation failure falls back and rebuilds"
    exit 0
}
printf '\n%s\n' "ok 1 - gridlut allocation failure falls back and rebuilds"
exit 0
