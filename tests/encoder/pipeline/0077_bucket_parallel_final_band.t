#!/bin/sh
# Verify bucket parallel final band.
# Policy: docs/encoder/output/encode-policy/README.md
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0077_bucket_parallel_final_band"
status=$?
set -e
test "$status" -ne 77 || {
    echo "ok 1 - bucket parallel final band # SKIP threads unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - bucket parallel final band"
    exit 0
}
echo "ok 1 - bucket parallel final band"
exit 0
