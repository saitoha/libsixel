#!/bin/sh
# Policy: docs/encoder/execution-map/README.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" 5bit/gpu_interleaved_consumers || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP required GPU backend unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - 5bit gpu interleaved consumers"
    exit 0
}
echo "ok 1 - 5bit gpu interleaved consumers"
exit 0
