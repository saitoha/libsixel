#!/bin/sh
# Policy: docs/encoder/execution-map/README.md
# gpu off prepares cpu lookup.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    encoder/indexed/gpu_lookup_preparation 3 || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - gpu off prepares cpu lookup"
    exit 0
}
echo "ok 1 - gpu off prepares cpu lookup"
exit 0
