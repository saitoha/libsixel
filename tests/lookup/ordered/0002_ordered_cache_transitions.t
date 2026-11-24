#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
# Policy alternation, palette resizing and rejected requests preserve exact results.
set -eux

echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup ordered \
    gpu-palette/0024_ordered_cache_transitions || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP GPU palette backend is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - ordered cache transitions"
    exit 0
}
printf '\n%s\n' "ok 1 - ordered cache transitions"
exit 0
