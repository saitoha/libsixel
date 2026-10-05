#!/bin/sh
# Verify delta byte fs keep.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0059_delta_byte_fs_keep || {
    echo "not ok 1 - delta byte fs keep"
    exit 0
}
echo "ok 1 - delta byte fs keep"
exit 0
