#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/fhedt.md
# Policy: docs/encoder/execution-map/README.md
# auto no cpu grid.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    encoder/indexed/gpu_lookup_preparation 1 fhedt || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - auto no cpu grid"
    exit 0
}
echo "ok 1 - auto no cpu grid"
exit 0
