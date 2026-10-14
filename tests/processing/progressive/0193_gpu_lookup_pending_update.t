#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Yield to a registered ordinary update after a real GPU lookup.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0193_gpu_lookup_pending_update"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu lookup pending update # SKIP threads or Metal unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu lookup pending update"
    exit 0
}
echo "ok 1 - gpu lookup pending update"
exit 0
