#!/bin/sh
# Verify mask size dense partial rows.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0092_mask_size_dense_partial_rows"
status=$?
set -e
test "$status" -ne 77 || {
    echo "ok 1 - mask size dense partial rows # SKIP threads unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - mask size dense partial rows"
    exit 0
}
echo "ok 1 - mask size dense partial rows"
exit 0
