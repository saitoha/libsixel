#!/bin/sh
# Verify bucket offset band boundary.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0072_bucket_offset_band_boundary"
status=$?
set -e
test "$status" -ne 77 || {
    echo "ok 1 - bucket offset band boundary # SKIP threads unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - bucket offset band boundary"
    exit 0
}
echo "ok 1 - bucket offset band boundary"
exit 0
