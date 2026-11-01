#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
# Sequential delta frames preserve indexes, keep masks and retained RGB across threshold and palette changes.
set -eux

echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette \
    gpu-palette/0026_ordered_delta_history || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - ordered delta history"
    exit 0
}
echo "ok 1 - ordered delta history"
exit 0
