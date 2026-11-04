#!/bin/sh
# Verify mask dense vector 32.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0083_mask_dense_vector_32"
status=$?
set -e
test "$status" -ne 77 || {
    echo "ok 1 - mask dense vector 32 # SKIP threads unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - mask dense vector 32"
    exit 0
}
echo "ok 1 - mask dense vector 32"
exit 0
