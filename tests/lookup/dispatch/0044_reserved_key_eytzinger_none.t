#!/bin/sh
# Verify reserved key eytzinger none.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup eytzinger \
    "lookup/dispatch/0044_reserved_key_eytzinger_none" || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - reserved key eytzinger none"
    exit 0
}
echo "ok 1 - reserved key eytzinger none"
exit 0
