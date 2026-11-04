#!/bin/sh
# Verify mask vector tail 17.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0082_mask_vector_tail_17"
status=$?
set -e
test "$status" -ne 77 || {
    echo "ok 1 - mask vector tail 17 # SKIP threads unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - mask vector tail 17"
    exit 0
}
echo "ok 1 - mask vector tail 17"
exit 0
