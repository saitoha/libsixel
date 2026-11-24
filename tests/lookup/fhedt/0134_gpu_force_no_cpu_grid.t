#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/fhedt.md
# Policy: docs/encoder/execution-map/README.md
# force no cpu grid.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    encoder/indexed/gpu_lookup_preparation 0 fhedt || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP GPU palette backend unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - force no cpu grid"
    exit 0
}
printf '\n%s\n' "ok 1 - force no cpu grid"
exit 0
