#!/bin/sh
# Policy: docs/encoder/execution-map/README.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu/blockcull_batch_opt_in || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU blockcull lookup unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - blockcull batch opt in"
    exit 0
}
echo "ok 1 - blockcull batch opt in"
exit 0
