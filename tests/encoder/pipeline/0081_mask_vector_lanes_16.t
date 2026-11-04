#!/bin/sh
# Verify mask vector lanes 16.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0081_mask_vector_lanes_16"
status=$?
set -e
test "$status" -ne 77 || {
    echo "ok 1 - mask vector lanes 16 # SKIP threads unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - mask vector lanes 16"
    exit 0
}
echo "ok 1 - mask vector lanes 16"
exit 0
